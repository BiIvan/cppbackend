#include <gtest/gtest.h>

#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "../src/controller.h"

class ControllerTest : public testing::Test {
protected:
    void Run(std::string commands) {
        input_.str(std::move(commands));
        input_.clear();

        output_.str("");
        output_.clear();

        menu_.Run();
    }

    void ExpectOutput(std::string_view expected) const {
        EXPECT_EQ(output_.str(), std::string{expected});
    }

    void ExpectNoArgsError(std::string_view command) const {
        ExpectOutput(
            std::string{"Error: the "} + std::string{command}
            + " command does not require any arguments\n");
    }

    TV tv_;
    std::istringstream input_;
    std::ostringstream output_;
    Menu menu_{input_, output_};
    Controller controller_{tv_, menu_};
};

TEST_F(ControllerTest, InfoWhenOff) {
    Run("Info");

    ExpectOutput("TV is turned off\n");
    EXPECT_FALSE(tv_.IsTurnedOn());
}

TEST_F(ControllerTest, InfoWhenOnShowsCurrentChannel) {
    tv_.TurnOn();
    tv_.SelectChannel(42);

    Run("Info");

    ExpectOutput("TV is turned on\nChannel number is 42\n");
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});
}

TEST_F(ControllerTest, TurnOnAndTurnOffProduceNoOutput) {
    Run("TurnOn");
    EXPECT_TRUE(tv_.IsTurnedOn());
    ExpectOutput("");

    Run("TurnOff");
    EXPECT_FALSE(tv_.IsTurnedOn());
    ExpectOutput("");
}

TEST_F(ControllerTest, RepeatedPowerCommandsDoNotResetHistory) {
    Run(
        "TurnOff\n"
        "TurnOff\n"
        "TurnOn\n"
        "SelectChannel 8\n"
        "SelectChannel 42\n"
        "TurnOn\n"
        "TurnOff\n"
        "TurnOff\n"
        "TurnOn\n"
        "SelectPreviousChannel\n");

    ExpectOutput("");
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
}

TEST_F(ControllerTest, CommandsWithoutArgumentsRejectExtraArguments) {
    for (bool turned_on : {false, true}) {
        for (const char* command : {
                 "Info",
                 "TurnOn",
                 "TurnOff",
                 "SelectPreviousChannel",
             }) {
            SCOPED_TRACE(turned_on);
            SCOPED_TRACE(command);

            tv_.TurnOn();
            tv_.SelectChannel(8);
            tv_.SelectChannel(42);

            if (!turned_on) {
                tv_.TurnOff();
            }

            Run(std::string{command} + " extra arguments");

            ExpectNoArgsError(command);
            EXPECT_EQ(tv_.IsTurnedOn(), turned_on);

            tv_.TurnOn();
            EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});

            tv_.SelectLastViewedChannel();
            EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
        }
    }
}

TEST_F(ControllerTest, CommandsAcceptSurroundingWhitespace) {
    Run(
        " \tTurnOn \t\n"
        " \tSelectChannel \t8 \t\n"
        " \tSelectPreviousChannel \t\n"
        " \tInfo \t\n"
        " \tTurnOff \t\n"
        " \tInfo \t\n");

    ExpectOutput(
        "TV is turned on\n"
        "Channel number is 1\n"
        "TV is turned off\n");

    EXPECT_FALSE(tv_.IsTurnedOn());
}

TEST_F(ControllerTest, CanSelectEveryValidChannel) {
    tv_.TurnOn();

    for (int channel = TV::MIN_CHANNEL;
         channel <= TV::MAX_CHANNEL; ++channel) {
        SCOPED_TRACE(channel);

        Run("SelectChannel " + std::to_string(channel));

        ExpectOutput("");
        EXPECT_EQ(tv_.GetChannel(), std::optional<int>{channel});
    }
}

TEST_F(ControllerTest, AcceptsSupportedIntegerSpellings) {
    tv_.TurnOn();

    for (const char* command : {
             "SelectChannel +8",
             "SelectChannel 08",
             "SelectChannel 8",
         }) {
        SCOPED_TRACE(command);

        Run(command);

        ExpectOutput("");
        EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
    }
}

TEST_F(ControllerTest, InvalidSyntaxDoesNotChangeStateOrHistory) {
    for (bool turned_on : {false, true}) {
        for (const char* command : {
                 "SelectChannel",
                 "SelectChannel   ",
                 "SelectChannel abc",
                 "SelectChannel 8abc",
                 "SelectChannel 8.5",
                 "SelectChannel 8 9",
                 "SelectChannel 8 extra",
                 "SelectChannel +",
                 "SelectChannel --8",
                 "SelectChannel 999999999999999999999999999999",
             }) {
            SCOPED_TRACE(turned_on);
            SCOPED_TRACE(command);

            tv_.TurnOn();
            tv_.SelectChannel(8);
            tv_.SelectChannel(42);

            if (!turned_on) {
                tv_.TurnOff();
            }

            Run(command);

            ExpectOutput("Invalid channel\n");
            EXPECT_EQ(tv_.IsTurnedOn(), turned_on);

            tv_.TurnOn();
            EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});

            tv_.SelectLastViewedChannel();
            EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
        }
    }
}

TEST_F(ControllerTest, OutOfRangeChannelDoesNotChangeHistory) {
    tv_.TurnOn();
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);

    for (const char* command : {
             "SelectChannel -1",
             "SelectChannel 0",
             "SelectChannel 100",
         }) {
        SCOPED_TRACE(command);

        Run(command);

        ExpectOutput("Channel is out of range\n");
        EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});
    }

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
}

TEST_F(ControllerTest, ChannelSelectionWhenOffReportsPowerState) {
    for (const char* command : {
             "SelectChannel 8",
             "SelectChannel 0",
             "SelectChannel 100",
             "SelectPreviousChannel",
         }) {
        SCOPED_TRACE(command);

        Run(command);

        ExpectOutput("TV is turned off\n");
        EXPECT_FALSE(tv_.IsTurnedOn());
        EXPECT_EQ(tv_.GetChannel(), std::nullopt);
    }
}

TEST_F(ControllerTest, PreviousChannelBeforeSelectionDoesNothing) {
    Run("TurnOn\nSelectPreviousChannel\nSelectPreviousChannel");

    ExpectOutput("");
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{1});
}

TEST_F(ControllerTest, PreviousChannelAlternatesBetweenLastTwoChannels) {
    Run(
        "TurnOn\n"
        "SelectChannel 8\n"
        "SelectChannel 42\n"
        "SelectPreviousChannel\n"
        "Info\n"
        "SelectPreviousChannel\n"
        "Info\n");

    ExpectOutput(
        "TV is turned on\n"
        "Channel number is 8\n"
        "TV is turned on\n"
        "Channel number is 42\n");
}

TEST_F(ControllerTest, SelectingCurrentChannelPreservesHistory) {
    Run(
        "TurnOn\n"
        "SelectChannel 8\n"
        "SelectChannel 42\n"
        "SelectChannel 42\n"
        "SelectPreviousChannel\n");

    ExpectOutput("");
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
}

TEST_F(ControllerTest, ErrorsDoNotStopProcessingFollowingCommands) {
    Run(
        "SelectChannel 8\n"
        "TurnOn extra\n"
        "TurnOn\n"
        "SelectChannel abc\n"
        "SelectChannel 100\n"
        "SelectChannel 42\n"
        "Info\n");

    ExpectOutput(
        "TV is turned off\n"
        "Error: the TurnOn command does not require any arguments\n"
        "Invalid channel\n"
        "Channel is out of range\n"
        "TV is turned on\n"
        "Channel number is 42\n");

    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});
}

TEST_F(ControllerTest, UnknownCommandDoesNotStopMenu) {
    Run("Unknown\nTurnOn\nInfo");

    ExpectOutput(
        "Command 'Unknown' has not been found.\n"
        "TV is turned on\n"
        "Channel number is 1\n");
}