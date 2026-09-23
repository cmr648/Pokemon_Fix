/*  Route 2 Roamer Hunter
 *
 *  Custom program for cycling the Route 2 gatehouse while Max Repel is active.
 */

#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_Superscalar.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_Route2RoamerHunter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

Route2RoamerHunter_Descriptor::Route2RoamerHunter_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:Route2RoamerHunter",
        Pokemon::STRING_POKEMON + " FRLG", "Route 2 Roamer Hunter",
        "Programs/PokemonFRLG/Route2RoamerHunter.html",
        "Cycle Route 2 and its gatehouse with Max Repel active until a roamer encounter begins.",
        ProgramControllerClass::StandardController_NoRestrictions,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

struct Route2RoamerHunter_Descriptor::Stats : public StatsTracker{
    Stats()
        : loops(m_stats["Route 2 Loops"])
        , repels(m_stats["Max Repels Used"])
        , encounters(m_stats["Encounters"])
    {
        m_display_order.emplace_back("Route 2 Loops");
        m_display_order.emplace_back("Max Repels Used");
        m_display_order.emplace_back("Encounters");
    }
    std::atomic<uint64_t>& loops;
    std::atomic<uint64_t>& repels;
    std::atomic<uint64_t>& encounters;
};

std::unique_ptr<StatsTracker> Route2RoamerHunter_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

Route2RoamerHunter::Route2RoamerHunter()
    : LEG_DURATION(
        "<b>Movement per Direction:</b><br>Run north or south for this long. The default is tuned for the Route 2 gatehouse loop shown in the reference video.",
        LockMode::LOCK_WHILE_RUNNING,
        "4500 ms"
    )
    , NOTIFICATION_ENCOUNTER(
        "Roamer encounter found",
        true, true, ImageAttachmentMode::JPG,
        {"Notifs", "Showcase"}
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_ENCOUNTER,
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
    })
{
    PA_ADD_OPTION(LEG_DURATION);
    PA_ADD_OPTION(NOTIFICATIONS);
}

Route2RoamerHunter::MoveResult Route2RoamerHunter::move_and_watch(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    bool north
) const{
    // Check before sending any input. Repel can expire on the single doorway
    // step, and an immediate B press would otherwise dismiss the message
    // before the detector's hold time completes.
    {
        BattleMenuWatcher battle_menu(COLOR_RED);
        WhiteDialogWatcher white_dialog(COLOR_RED);
        context.wait_for_all_requests();
        int ret = wait_until(
            env.console, context, 600ms,
            {battle_menu, white_dialog}
        );
        if (ret == 0){
            return MoveResult::encounter;
        }
        if (ret == 1){
            return MoveResult::repel_expired;
        }
    }

    // The generic battle-dialog detector can falsely match the dark doorway
    // transition when re-entering the Route 2 gatehouse.  The battle-menu
    // detector requires the actual FIGHT/POKEMON/BAG/RUN screen, so normal
    // map transitions do not stop the loop.
    BattleMenuWatcher battle_menu(COLOR_RED);
    WhiteDialogWatcher white_dialog(COLOR_RED);
    Milliseconds duration = LEG_DURATION;
    Milliseconds run_duration = duration > 64ms ? duration - 64ms : duration;

    context.wait_for_all_requests();
    int ret = run_until<ProControllerContext>(
        env.console, context,
        [north, duration, run_duration](ProControllerContext& context){
            if (north){
                ssf_press_left_joystick(context, {0, +1}, 0ms, duration);
            }else{
                ssf_press_left_joystick(context, {0, -1}, 0ms, duration);
            }
            // Hold B to run. Do not mash it: repeated B presses can dismiss
            // the Repel-expired message before vision confirms it.
            ssf_press_button(context, BUTTON_B, 0ms, run_duration);
        },
        {battle_menu, white_dialog}
    );
    context.wait_for_all_requests();

    if (ret == 0){
        return MoveResult::encounter;
    }
    if (ret == 1){
        return MoveResult::repel_expired;
    }
    return MoveResult::completed;
}

void Route2RoamerHunter::step_through_door(
    ProControllerContext& context,
    bool north
) const{
    // Send only a short, one-tile movement input.  Direction is released
    // before the map transition begins, so loading time cannot change how far
    // the player travels on either map.
    if (north){
        pbf_move_left_joystick(context, {0, +1}, 250ms, 1750ms);
    }else{
        pbf_move_left_joystick(context, {0, -1}, 250ms, 1750ms);
    }
    context.wait_for_all_requests();
}

void Route2RoamerHunter::return_to_gatehouse(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
) const{
    env.log("Re-anchoring at the Route 2 gatehouse after reusing Max Repel...");
    BlackScreenWatcher doorway(COLOR_BLUE);

    context.wait_for_all_requests();
    run_until<ProControllerContext>(
        env.console, context,
        [](ProControllerContext& context){
            ssf_press_left_joystick(context, {0, -1}, 0ms, 15000ms);
            ssf_press_button(context, BUTTON_B, 0ms, 14936ms);
        },
        {doorway}
    );
    context.wait_for_all_requests();

    // The doorway watcher fires during the fade.  Wait until the gatehouse is
    // fully loaded.  The player is now one step inside the north doorway.
    pbf_wait(context, 1750ms);
    context.wait_for_all_requests();
}

void Route2RoamerHunter::reuse_max_repel(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context
) const{
    env.log("Max Repel expired. Reusing the last-selected Max Repel...");

    // Dismiss "REPEL's effect wore off." and open the Bag. The game remembers
    // the last-selected item, so Max Repel must be selected before starting.
    pbf_press_button(context, BUTTON_B, 200ms, 800ms);
    open_bag_from_overworld(env.console, context);
    pbf_mash_button(context, BUTTON_A, 1800ms);
    pbf_mash_button(context, BUTTON_B, 3000ms);
    context.wait_for_all_requests();
}

void Route2RoamerHunter::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    Route2RoamerHunter_Descriptor::Stats& stats = env.current_stats<Route2RoamerHunter_Descriptor::Stats>();

    env.log("Starting Route 2 roamer loop.");
    env.log("Any detected battle will stop the program without selecting a battle command.");

    while (true){
        // Start one tile inside the north doorway.  Door movement is kept
        // separate from the timed outdoor legs so every loop has identical
        // Route 2 travel distance regardless of map-loading time.
        step_through_door(context, true);

        MoveResult result = move_and_watch(env, context, true);

        if (result == MoveResult::encounter){
            stats.encounters++;
            env.update_stats();
            env.log("Encounter detected. Stopping all inputs and leaving the battle untouched.", COLOR_YELLOW);
            send_program_notification(
                env,
                NOTIFICATION_ENCOUNTER,
                COLOR_YELLOW,
                "Encounter detected on Route 2. The battle has been left untouched.",
                {}, "",
                env.console.video().snapshot(),
                true
            );
            break;
        }

        if (result == MoveResult::repel_expired){
            reuse_max_repel(env, context);
            stats.repels++;
            env.update_stats();
            return_to_gatehouse(env, context);
            continue;
        }

        result = move_and_watch(env, context, false);

        if (result == MoveResult::encounter){
            stats.encounters++;
            env.update_stats();
            env.log("Encounter detected. Stopping all inputs and leaving the battle untouched.", COLOR_YELLOW);
            send_program_notification(
                env,
                NOTIFICATION_ENCOUNTER,
                COLOR_YELLOW,
                "Encounter detected on Route 2. The battle has been left untouched.",
                {}, "",
                env.console.video().snapshot(),
                true
            );
            break;
        }

        if (result == MoveResult::repel_expired){
            reuse_max_repel(env, context);
            stats.repels++;
            env.update_stats();
            return_to_gatehouse(env, context);
            continue;
        }

        // We are back on the outdoor doorway tile.  Take exactly one step
        // inside, wait for the transition, and begin the next anchored loop.
        step_through_door(context, false);
        stats.loops++;
        env.update_stats();
        send_program_status_notification(env, NOTIFICATION_STATUS_UPDATE);
    }

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
}

}
}
}
