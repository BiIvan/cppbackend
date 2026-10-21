#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <stdexcept>

#include "../src/tv.h"

class TVByDefault : public testing::Test {
protected:
    TV tv_;
};

TEST_F(TVByDefault, IsOffAndHasNoVisibleChannel) {
    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}

TEST_F(TVByDefault, FirstTurnOnSelectsChannelOne) {
    tv_.TurnOn();

    EXPECT_TRUE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{1});
}

TEST_F(TVByDefault, RepeatedTurnOffDoesNothing) {
    tv_.TurnOff();
    tv_.TurnOff();

    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);

    tv_.TurnOn();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{1});
}

TEST_F(TVByDefault, CannotSelectChannelsWhenOff) {
    for (int channel : {0, 1, 42, 99, 100}) {
        SCOPED_TRACE(channel);
        EXPECT_THROW(tv_.SelectChannel(channel), std::logic_error);
    }

    EXPECT_EQ(tv_.GetChannel(), std::nullopt);

    tv_.TurnOn();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{1});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{1});
}

TEST_F(TVByDefault, CannotSelectPreviousChannelWhenOff) {
    EXPECT_THROW(tv_.SelectLastViewedChannel(), std::logic_error);
    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}

class TurnedOnTV : public TVByDefault {
protected:
    void SetUp() override {
        tv_.TurnOn();
    }
};

TEST_F(TurnedOnTV, CanSelectEveryValidChannel) {
    for (int channel = TV::MIN_CHANNEL;
         channel <= TV::MAX_CHANNEL; ++channel) {
        SCOPED_TRACE(channel);

        EXPECT_NO_THROW(tv_.SelectChannel(channel));
        EXPECT_TRUE(tv_.IsTurnedOn());
        EXPECT_EQ(tv_.GetChannel(), std::optional<int>{channel});
    }
}

TEST_F(TurnedOnTV, RejectsInvalidChannelsWithoutChangingHistory) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);

    for (int channel : {
             std::numeric_limits<int>::min(),
             -1,
             0,
             100,
             std::numeric_limits<int>::max(),
         }) {
        SCOPED_TRACE(channel);

        EXPECT_THROW(tv_.SelectChannel(channel), std::out_of_range);
        EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});
    }

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});
}

TEST_F(TurnedOnTV, TurnOffHidesChannel) {
    tv_.SelectChannel(42);
    tv_.TurnOff();

    EXPECT_FALSE(tv_.IsTurnedOn());
    EXPECT_EQ(tv_.GetChannel(), std::nullopt);
}

TEST_F(TurnedOnTV, RepeatedTurnOnDoesNotResetCurrentChannelOrHistory) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);

    tv_.TurnOn();
    tv_.TurnOn();

    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
}

TEST_F(TurnedOnTV, PowerCyclePreservesCurrentChannelAndHistory) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);

    tv_.TurnOff();
    tv_.TurnOff();
    tv_.TurnOn();

    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
}

TEST_F(TurnedOnTV, PreviousChannelBeforeAnySelectionIsOne) {
    tv_.SelectLastViewedChannel();
    tv_.SelectLastViewedChannel();

    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{1});
}

TEST_F(TurnedOnTV, PreviousChannelAlternatesBetweenLastTwoChannels) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);
    tv_.SelectChannel(99);

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{99});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});
}

TEST_F(TurnedOnTV, SelectingCurrentChannelDoesNotOverwriteHistory) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);
    tv_.SelectChannel(42);

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
}

TEST_F(TurnedOnTV, SelectingAfterPreviousChannelUpdatesHistory) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);
    tv_.SelectLastViewedChannel();

    tv_.SelectChannel(99);
    tv_.SelectLastViewedChannel();

    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{99});
}

TEST_F(TurnedOnTV, FailedOperationsWhileOffPreserveHistory) {
    tv_.SelectChannel(8);
    tv_.SelectChannel(42);
    tv_.TurnOff();

    EXPECT_THROW(tv_.SelectChannel(99), std::logic_error);
    EXPECT_THROW(tv_.SelectLastViewedChannel(), std::logic_error);

    tv_.TurnOn();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{42});

    tv_.SelectLastViewedChannel();
    EXPECT_EQ(tv_.GetChannel(), std::optional<int>{8});
}