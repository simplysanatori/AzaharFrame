// Copyright 2026 AzaharFrame contributors
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

// AzaharFrame flat2: one XrInstance per VR session (frame_xr/instance_policy.h), as the frame loop
// drives it over VR <-> flat round trips.

#include <catch2/catch_test_macros.hpp>
#include "frame_xr/instance_policy.h"

using namespace FrameXr;

namespace {

// What the frame loop does: Start makes the instance (and the session in VR); "VR again" renews
// it when asked and makes a session; the end of a session ends the instance when asked.
struct Loop {
    InstanceSessions policy;
    int instances_made = 0;
    uint32_t last_session_on_instance = 0;

    explicit Loop(InstanceMode mode) : policy{mode} {
        MakeInstance();
    }
    void MakeInstance() {
        policy.InstanceMade();
        instances_made++;
    }
    void Vr() { // the !session branch with want_vr: RenewInstance, then CreateSession
        if (policy.NeedsNewInstance()) {
            policy.InstanceGone();
            MakeInstance();
        }
        last_session_on_instance = policy.SessionMade();
    }
    void Flat() { // the session ended (flat asked for)
        if (policy.EndInstanceAfterSession()) {
            policy.InstanceGone();
        }
    }
};

} // namespace

TEST_CASE("InstanceModeFrom: keep only when asked", "[frame_xr]") {
    REQUIRE(InstanceModeFrom(nullptr) == InstanceMode::PerSession);
    REQUIRE(InstanceModeFrom("") == InstanceMode::PerSession);
    REQUIRE(InstanceModeFrom("session") == InstanceMode::PerSession);
    REQUIRE(InstanceModeFrom("keep") == InstanceMode::KeepForRun);
}

TEST_CASE("InstanceSessions: VR start, three round trips, every session first on its instance",
          "[frame_xr]") {
    Loop loop{InstanceMode::PerSession};
    loop.Vr(); // the session Start makes
    REQUIRE(loop.last_session_on_instance == 1);
    REQUIRE(loop.instances_made == 1);
    for (int trip = 1; trip <= 3; trip++) {
        loop.Flat();
        REQUIRE(loop.policy.NeedsNewInstance()); // flat holds no instance
        REQUIRE_FALSE(loop.policy.NextSessionReused());
        loop.Vr();
        REQUIRE(loop.last_session_on_instance == 1);
        REQUIRE(loop.instances_made == 1 + trip);
    }
}

TEST_CASE("InstanceSessions: a flat start uses the start's instance for its first VR session",
          "[frame_xr]") {
    Loop loop{InstanceMode::PerSession};
    REQUIRE_FALSE(loop.policy.NeedsNewInstance()); // made at start, no session yet
    loop.Vr();
    REQUIRE(loop.instances_made == 1);
    REQUIRE(loop.last_session_on_instance == 1);
    loop.Flat();
    loop.Vr();
    REQUIRE(loop.instances_made == 2);
    REQUIRE(loop.last_session_on_instance == 1);
}

TEST_CASE("InstanceSessions: a failed session leaves no instance behind", "[frame_xr]") {
    InstanceSessions policy;
    policy.InstanceMade();
    // RenewInstance passed, CreateSession failed: the loop ends the instance.
    REQUIRE(policy.EndInstanceAfterSession());
    policy.InstanceGone();
    REQUIRE(policy.NeedsNewInstance());
}

TEST_CASE("InstanceSessions: keep reuses one instance (the 1.1 behaviour) and says so",
          "[frame_xr]") {
    Loop loop{InstanceMode::KeepForRun};
    loop.Vr();
    REQUIRE(loop.policy.NextSessionReused()); // a second session would reuse it
    for (int trip = 1; trip <= 3; trip++) {
        loop.Flat();
        REQUIRE_FALSE(loop.policy.NeedsNewInstance());
        REQUIRE(loop.policy.NextSessionReused());
        loop.Vr();
        REQUIRE(loop.last_session_on_instance == static_cast<uint32_t>(1 + trip));
    }
    REQUIRE(loop.instances_made == 1);
}
