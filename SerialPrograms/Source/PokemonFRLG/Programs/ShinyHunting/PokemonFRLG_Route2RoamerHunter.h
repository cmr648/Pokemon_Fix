/*  Route 2 Roamer Hunter
 *
 *  Custom program for cycling the Route 2 gatehouse while Max Repel is active.
 */

#ifndef PokemonAutomation_PokemonFRLG_Route2RoamerHunter_H
#define PokemonAutomation_PokemonFRLG_Route2RoamerHunter_H

#include "CommonFramework/Notifications/EventNotificationsTable.h"
#include "Common/Cpp/Options/SimpleIntegerOption.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

class Route2RoamerHunter_Descriptor : public SingleSwitchProgramDescriptor{
public:
    Route2RoamerHunter_Descriptor();
    struct Stats;
    virtual std::unique_ptr<StatsTracker> make_stats() const override;
};

class Route2RoamerHunter : public SingleSwitchProgramInstance{
public:
    using Descriptor = Route2RoamerHunter_Descriptor;
    Route2RoamerHunter();
    virtual void program(SingleSwitchProgramEnvironment& env, ProControllerContext& context) override;

    virtual void start_program_border_check(VideoStream&, FeedbackType) override{}

private:
    enum class MoveResult{
        completed,
        encounter,
        repel_expired,
    };

    MoveResult move_and_watch(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context,
        bool north
    ) const;
    void step_through_door(ProControllerContext& context, bool north) const;
    void return_to_gatehouse(
        SingleSwitchProgramEnvironment& env,
        ProControllerContext& context
    ) const;
    void reuse_max_repel(SingleSwitchProgramEnvironment& env, ProControllerContext& context) const;

    MillisecondsOption LEG_DURATION;
    EventNotificationOption NOTIFICATION_ENCOUNTER;
    EventNotificationOption NOTIFICATION_STATUS_UPDATE;
    EventNotificationsOption NOTIFICATIONS;
};

}
}
}
#endif
