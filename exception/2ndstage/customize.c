/*  customize.c

DESCRIPTION

    Customization reverse engineering helper code.

TODO

    Versus mode needs to be reversed.

    Cable versus needs to be tested.

    Menu select side should be checked instead of game side in Cable Versus mode.

    Re-reverse the entire system to have data transmitted across the line.
    Customized heads are *for certain* not transmitted.

*/

#include "vars.h"
#include "exception.h"
#include "exception-lowlevel.h"
#include "util.h"
#include "gamedata.h"
#include "printf.h"
#include "controller.h"
#include "vmu.h"

#include "customize.h"

static char module_save[VMU_MAX_FILENAME];
static const uint8 osd_func_key[] = { 0xe6, 0x2f, 0xd6, 0x2f, 0xc6, 0x2f, 0xb6, 0x2f, 0xa6, 0x2f, 0x96, 0x2f, 0x86, 0x2f, 0xfb, 0xff, 0x22, 0x4f, 0xfc, 0x7f, 0x43, 0x6d, 0x00, 0xe4, 0x43, 0x6b };

static const uint8 custom_func_key[] = { 0xe6, 0x2f, 0x53, 0x6e, 0xd6, 0x2f, 0xef, 0x63, 0xc6, 0x2f, 0x38, 0x23 };
static uint32 custom_func;

static customize_ipc ipc = C_IPC_START;
static uint8 player = 0;
static uint8 side = 0;
static uint8 file_number = 0;
static uint8 *file_buffer = NULL;
static customize_data* colors[2][VR_SENTINEL]; 

void customize_init(void)
{
    exception_table_entry new;

    /* STAGE: Break on the secondary animation vector. */
    *UBC_R_BARA = 0x8ccf022a;
    *UBC_R_BAMRA = UBC_BAMR_NOASID;
    *UBC_R_BBRA = UBC_BBR_READ | UBC_BBR_OPERAND;

    ubc_wait();

    /* STAGE: Add exception handler for serial access. */
    new.type = EXP_TYPE_GEN;
    new.code = EXP_CODE_UBC;
    new.handler = customize_handler;

    add_exception_handler(&new);
}

static void customize_clear_player(uint32 player, bool do_head)
{
    uint32 vr;
    uint8 *cust_head = (uint8 *) (0x8ccf9ecc + 0x4b);

    /* STAGE: Free and null out all the customization for a particular player. */
    for (vr = 0; vr < VR_SENTINEL ; vr++)
    {
        if (colors[player][vr])
        {
            free(colors[player][vr]);
            colors[player][vr] = NULL;
        }
    }

    if (do_head)
        cust_head[player] = 0x0;
}

static void* my_customize_handler(register_stack *stack, void *current_vector)
{
    voot_vr_id vr;
    uint32 player;

    vr = stack->r4;
    player = stack->r5;

    /* STAGE: Ensure we've been given sane values. */
    if ((vr >= 0 && vr < VR_SENTINEL) && (player == 0 || player == 1))
    {
        if (colors[player][vr])
        {
            uint8 *cust_head = (uint8 *) (0x8ccf9ecc + 0x4b);

            memcpy((uint8 *) stack->r6, colors[player][vr]->palette, sizeof(customize_data));
            cust_head[player] = colors[player][vr]->head;
        }

        customize_clear_player(player, FALSE);
    }

    return current_vector;
}

static void maybe_start_load_customize(void)
{
    uint8 *control_port = (uint8 *) 0x8ccf9f1a;
    uint8 *menu_side = (uint8 *) 0x8ccf9f2e;
    uint8 *game_side = (uint8 *) 0x8ccf96f8;
    vmu_port *data_port = (vmu_port *) (0x8ccf9f06);

    /* STAGE: [Step 1-P1] First player requests customization load. */
    if (ipc == C_IPC_START && (check_controller_press(CONTROLLER_PORT_A0) & CONTROLLER_MASK_BUTTON_Y))
    {
        /* STAGE: If the control port is 0 (A), our side is equal to the DNA/RNA select.
                  If the control port is 1 (B), our side is equal to the reverse of the DNA/RNA select.
                  Our player is the controlling port.
        */
                 
        if (*control_port)
            side = !(*game_side);
        else
            side = *game_side;
        player = *control_port;

        /* STAGE: Make sure the colors for this player are cleared out. */
        customize_clear_player(player, TRUE); 

        /* STAGE: Begin the mount scan for a VMS. */
        *data_port = VMU_PORT_A1;
        ipc = C_IPC_MOUNT_SCAN_1;
        file_number = 0;

        /* STAGE: Let them know we're loading the data. */
        if (side)
            VIDEO_BORDER_COLOR = VIDEO_COLOR_RED;
        else
            VIDEO_BORDER_COLOR = VIDEO_COLOR_BLUE;

        vmu_mount(*data_port);
    }
    /* STAGE: [Step 1-P2] Second player requests customization load. */
    else if (ipc == C_IPC_START && (check_controller_press(CONTROLLER_PORT_B0) & CONTROLLER_MASK_BUTTON_Y))
    {
        /* STAGE: If the control port is 0 (A), our side is equal to the reverse of the DNA/RNA select.
                  If the control port is 1 (B), our side is equal to DNA/RNA select.
                  Our player is the reverse of the controlling port.
        */
                 
        if (!(*control_port))
            side = !(*game_side);
        else
            side = *game_side;
        player = !(*control_port);

        /* STAGE: Make sure the colors for this player are cleared out. */
        customize_clear_player(player, TRUE); 

        /* STAGE: Begin the mount scan for a VMS. */
        *data_port = VMU_PORT_B1;
        ipc = C_IPC_MOUNT_SCAN_1;
        file_number = 0;

        /* STAGE: Let them know we're loading the data. */
        if (side)
            VIDEO_BORDER_COLOR = VIDEO_COLOR_RED;
        else
            VIDEO_BORDER_COLOR = VIDEO_COLOR_BLUE;

        vmu_mount(*data_port);
    }
}

static void maybe_do_load_customize(void)
{
    vmu_port *data_port = (vmu_port *) (0x8ccf9f06);

    /* STAGE: [Step 2] If we're involved in either step of the mount scan. */
    if ((ipc == C_IPC_MOUNT_SCAN_1 || ipc == C_IPC_MOUNT_SCAN_2) && !vmu_status(*data_port))
    {
        uint32 retval;
        char filename[VMU_MAX_FILENAME];

        /* STAGE: Determine if we're mounted by searching for a customization file. */
        for (retval = 1; file_number < 100; file_number++)
        {
            snprintf(filename, sizeof(filename), "VOORATAN.C%02u", file_number);

            retval = vmu_exists_file(*data_port, filename);

            if (!retval)
                break;
        }

        /* STAGE: We found the file! Now lets start accessing it... */
        if (!retval)
        {
            /* STAGE: Give us a 10 block temporary file buffer. */
            file_buffer = malloc(512 * CUSTOMIZE_VMU_SIZE);

            /* STAGE: Make sure we actually obtained the file buffer. */
            if (file_buffer)
            {
                vmu_load_file(*data_port, filename, file_buffer, CUSTOMIZE_VMU_SIZE);

                /* STAGE: And we're all done loading... */
                VIDEO_BORDER_COLOR = VIDEO_COLOR_WHITE;

                ipc = C_IPC_LOAD;
            }
            else
                ipc = C_IPC_START;

        }
        /* STAGE: Didn't find a customization file on this VMU, so check the next port. */
        else if (ipc == C_IPC_MOUNT_SCAN_1)
        {
            (*data_port)++;
            file_number = 0;

            vmu_mount(*data_port);

            ipc = C_IPC_MOUNT_SCAN_2;
        }
        /* STAGE: Apparently it wasn't found on either port. Abort. */
        else if (ipc == C_IPC_MOUNT_SCAN_2)
        {
            *data_port = VMU_PORT_NONE;

            /* STAGE: And we're all done loading... */
            VIDEO_BORDER_COLOR = VIDEO_COLOR_BLACK;

            ipc = C_IPC_START;
        }
    }
    /* STAGE: [Step 3] Loaded customization file. */
    else if (ipc == C_IPC_LOAD && !vmu_status(*data_port))
    {
        voot_vr_id vr;

        vr = file_buffer[CUSTOMIZE_VMU_VR_IDX];

        /* STAGE: If it is one of the VR types we're storing, copy the data
            over. If we already stored one, skip it. */
        if (vr < VR_SENTINEL && !colors[player][vr])
        {
            customize_data temp_color;
            uint8 temp_head;

            /* STAGE: This whole trick is to keep memory from becoming very
                fragmented. With only 64k, it's good to keep this in mind. */

            memcpy(temp_color.palette, file_buffer + CUSTOMIZE_VMU_COLOR_IDX + (side * CUSTOMIZE_PALETTE_SIZE), CUSTOMIZE_PALETTE_SIZE);
            temp_head = file_buffer[CUSTOMIZE_VMU_HEAD_IDX];

            free(file_buffer);
            colors[player][vr] = malloc(sizeof(customize_data));

            /* STAGE: Make sure we actually obtained the memory for the customization. */
            if (colors[player][vr])
            {
                memcpy(colors[player][vr]->palette, temp_color.palette, CUSTOMIZE_PALETTE_SIZE);
                colors[player][vr]->head = temp_head;
            }
        }
        else
            free(file_buffer);

        file_number++;
        ipc = C_IPC_MOUNT_SCAN_2;
    }
}

static void* my_anim_handler(register_stack *stack, void *current_vector)
{
    uint16 *anim_mode_a = (uint16 *) 0x8ccf0228;
    uint16 *anim_mode_b = (uint16 *) 0x8ccf022a;
    static uint16 save_mode_a = 0;
    static uint16 save_mode_b = 0;
    static uint32 play_vector = 0;
    static uint32 osd_vector = 0;
    static int32 p1_health_save[2] = {-1, -1};
    static int32 p2_health_save[2] = {-1, -1};

    /* STAGE: This code occurs are every animation changeover. (game code latch) */
    if ((save_mode_a != *anim_mode_a || save_mode_b != *anim_mode_b))
    {
        /* STAGE: Detect module changeovers and search for the customization function. */
        if (memcmp((uint8 *) custom_func, custom_func_key, sizeof(custom_func_key)) && strcmp(module_save, (const char *) VOOT_MODULE_NAME))
        {
            strncpy(module_save, (const char *) VOOT_MODULE_NAME, sizeof(module_save));

            /* STAGE: Try to locate one of the customization functions. */
            custom_func = (uint32) search_sysmem_at(custom_func_key, sizeof(custom_func_key), GAME_MEM_START, SYS_MEM_END);

            /* STAGE: Place the breakpoint on the customization function, if we found it. */
            if (custom_func)
            {
                *UBC_R_BARB = custom_func;
                *UBC_R_BAMRB = UBC_BAMR_NOASID;
                *UBC_R_BBRB = UBC_BBR_READ | UBC_BBR_INSTRUCT;
            }
            else
                *UBC_R_BBRB = 0;

            ubc_wait();
        }

        /* STAGE: If we move back to the main menu, clear all the information. */
        if (*anim_mode_a == 0x2 && !(*anim_mode_b == 0x9 || *anim_mode_b == 0xa))
        {
            customize_clear_player(0, TRUE);
            customize_clear_player(1, TRUE);
        }

        /* STAGE: Make sure we don't catch next time around. */
        save_mode_a = *anim_mode_a;
        save_mode_b = *anim_mode_b;
    }

    /* STAGE: See if we need to load customization information. */
    if ((*anim_mode_a == 0x0 && *anim_mode_b == 0x2)  ||    /* Training Mode select. */
        (*anim_mode_a == 0x0 && *anim_mode_b == 0x5)  ||    /* Single Player 3d select. */
        (*anim_mode_a == 0x2 && *anim_mode_b == 0x9)  ||    /* Single Player quick select. */
        (*anim_mode_a == 0x3 && *anim_mode_b == 0x29) ||    /* Single Player quick continue. */
        (*anim_mode_a == 0x5 && *anim_mode_b == 0x2))       /* Versus select. */
    {
        uint8 *p1_vr_loc = (uint8 *) 0x8ccf6236;
        uint8 *p2_vr_loc = (uint8 *) 0x8ccf73b2;
        voot_vr_id p1_vr = VR_SENTINEL;
        voot_vr_id p2_vr = VR_SENTINEL;
        uint8 *cust_head = (uint8 *) (0x8ccf9ecc + 0x4b);

        /* STAGE: Handle customization load process. */
        maybe_start_load_customize();
        maybe_do_load_customize();

        /* STAGE: An ugly hack of an attempt at customized heads. */
        p1_vr = *p1_vr_loc;
        p2_vr = *p2_vr_loc;

        if (p1_vr >= 0 && p1_vr < VR_SENTINEL && colors[0][p1_vr])
            cust_head[0] = colors[0][p1_vr]->head;
        else
            cust_head[0] = 0x0;

        if (p2_vr >= 0 && p2_vr < VR_SENTINEL && colors[1][p2_vr])
            cust_head[1] = colors[1][p2_vr]->head;
        else
            cust_head[1] = 0x0;
    }
    else
    {
        /* STAGE: Handle the customization load process. (or cleanup) */
        maybe_do_load_customize();
    }

    /* STAGE: Health OSD segment - only triggers in game mode. */
    if (*anim_mode_b == 0xf && *anim_mode_a == 0x3)
    {
        if ((!play_vector || play_vector == spc()) && (!strcmp("training54.bin", (const char *) VOOT_MODULE_NAME) || !strcmp("training.bin", (const char *) VOOT_MODULE_NAME)))
        {
            uint16 *p1_health_real = (uint16 *) 0x8CCF6284;
            uint16 *p1_health_stat = (uint16 *) 0x8CCF6286;

            uint16 *p1_varmour_mod = (uint16 *) 0x8CCF63ec;
            uint16 *p1_varmour_base = (uint16 *) 0x8CCF63ee;

            uint16 *p2_health_real = (uint16 *) 0x8CCF7400;
            uint16 *p2_health_stat = (uint16 *) 0x8CCF7402;

            uint16 *p2_varmour_mod = (uint16 *) 0x8CCF7568;
            uint16 *p2_varmour_base = (uint16 *) 0x8CCF756a;

            char cbuffer[40];

            /* STAGE: So we only latch on a single animation vector. */
            play_vector = spc();

            /* STAGE: Locate the OSD vector. */
            if (!osd_vector)
                osd_vector = (uint32) search_sysmem_at(osd_func_key, sizeof(osd_func_key), GAME_MEM_START, SYS_MEM_END);

            /* STAGE: If we found it, display the health OSD. */
            if (osd_vector)
            {
                snprintf(cbuffer, sizeof(cbuffer), "Dam 1 [%u > %u | %d]", *p1_health_real, *p1_health_stat, p1_health_save[1]);
                (*(void (*)()) osd_vector)(5, 340, cbuffer);

                snprintf(cbuffer, sizeof(cbuffer), "V.A 1 [%u > %u]", *p1_varmour_base, *p1_varmour_mod);
                (*(void (*)()) osd_vector)(5, 355, cbuffer);

                snprintf(cbuffer, sizeof(cbuffer), "Dam 2 [%u > %u | %d]", *p2_health_real, *p2_health_stat, p2_health_save[1]);
                (*(void (*)()) osd_vector)(5, 400, cbuffer);

                snprintf(cbuffer, sizeof(cbuffer), "V.A 2 [%u > %u]", *p2_varmour_base, *p2_varmour_mod);
                (*(void (*)()) osd_vector)(5, 415, cbuffer);
            }

            /* STAGE: If the health values have changed, update our status. */
            if (*p1_health_real != p1_health_save[0])
            {
                if (p1_health_save[0] >= 0)
                    p1_health_save[1] = p1_health_save[0] - *p1_health_real;

                p1_health_save[0] = *p1_health_real;
            }
            if (*p2_health_real != p2_health_save[0])
            {
                if (p2_health_save[0] >= 0)
                    p2_health_save[1] = p2_health_save[0] - *p2_health_real;

                p2_health_save[0] = *p2_health_real;
            }
        }
    }
    else
    {
        play_vector = osd_vector = 0;
        p1_health_save[0] = p2_health_save[0] = -1;
        p1_health_save[1] = p2_health_save[1] = 0;
    }

    /* STAGE: Check if we're in a VR Customization module. If so, enable Button Y to start VR Test. */
    if (*anim_mode_a == 0x0 && *anim_mode_b == 0x0 && !strcmp("vrcust.bin", (const char *) 0x8ccf9edc))
    {
        vmu_port *data_port = (vmu_port *) (0x8ccf9f06);

        if (check_controller_press(*data_port) & CONTROLLER_MASK_BUTTON_Y)
        {
            uint8 *vrcust_action = (uint8 *) 0x8c275224;

            *vrcust_action = 0x1a;
        }
    }

    return current_vector;
}

void* customize_handler(register_stack *stack, void *current_vector)
{
    /* STAGE: In the case of the secondary animation mode (channel A) exception. */
    if (*UBC_R_BRCR & UBC_BRCR_CMFB)
    {
        /* STAGE: Be sure to clear the proper bit. */
        *UBC_R_BRCR &= ~(UBC_BRCR_CMFB);

        /* STAGE: Pass control to the actual code base. */
        current_vector = my_customize_handler(stack, current_vector);
    }

    if (*UBC_R_BRCR & UBC_BRCR_CMFA)
    {
        /* STAGE: Be sure to clear the proper bit. */
        *UBC_R_BRCR &= ~(UBC_BRCR_CMFA);

        /* STAGE: Pass control to the actual code base. */
        current_vector = my_anim_handler(stack, current_vector);
    }

    return current_vector;
}
