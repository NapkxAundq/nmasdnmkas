#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>
#include <array>
#include <deque>

using namespace geode::prelude;

namespace tickbuffer {
    constexpr std::array<int, 5> WINDOWS = {2, 4, 8, 16, 24};
    constexpr int OFF = static_cast<int>(WINDOWS.size());

    // Starts disabled; the first F6 press selects the 2-tick mode.
    inline int stage = OFF;

    inline bool verbose() {
        return Mod::get()->getSettingValue<bool>("verbose-log");
    }

    inline int currentWindow() {
        return stage == OFF ? 0 : WINDOWS[stage];
    }

    inline void announceStage() {
        if (stage == OFF) {
            log::info("\n[Tick Buffer] OFF\n");
            Notification::create("Tick Buffer\nOFF", NotificationIcon::Info)->show();
            return;
        }

        const int ticks = currentWindow();
        log::info("\n[Tick Buffer] mode: {} ticks\n", ticks);
        Notification::create(
            fmt::format("Tick Buffer\n{}-tick jump buffer", ticks),
            NotificationIcon::Success
        )->show();
    }
}

constexpr int TICK_BUFFER_JUMP_BUTTON = 1;

struct TickBufferPendingJump {
    bool player1;
    int remaining;
    bool released;
};

class $modify(TickBufferBaseLayer, GJBaseGameLayer) {
    struct Fields {
        std::deque<TickBufferPendingJump> pendingJumps;
        bool applyingBufferedInput = false;
    };

    void handleButton(bool down, int button, bool isPlayer1) {
        auto* fields = m_fields.self();

        if (fields->applyingBufferedInput || tickbuffer::stage == tickbuffer::OFF ||
            button != TICK_BUFFER_JUMP_BUTTON) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto* player = isPlayer1 ? m_player1 : m_player2;
        if (!player) {
            GJBaseGameLayer::handleButton(down, button, isPlayer1);
            return;
        }

        auto pending = fields->pendingJumps.end();
        for (auto it = fields->pendingJumps.begin(); it != fields->pendingJumps.end(); ++it) {
            if (it->player1 == isPlayer1) {
                pending = it;
                break;
            }
        }

        // Keep the release paired with a press that has not reached the game yet.
        if (!down && pending != fields->pendingJumps.end()) {
            pending->released = true;
            if (tickbuffer::verbose()) {
                log::info("[Tick Buffer] saved release for pending jump");
            }
            return;
        }

        // Buffer only an early primary-button press. Everything else is passed on unchanged.
        if (down && !player->m_isDart && !player->m_isOnGround) {
            const int window = tickbuffer::currentWindow();
            if (pending != fields->pendingJumps.end()) {
                pending->remaining = window;
                pending->released = false;
            } else {
                fields->pendingJumps.push_back({isPlayer1, window, false});
            }

            if (tickbuffer::verbose()) {
                log::info("[Tick Buffer] queued early jump for {} physics ticks", window);
            }
            return;
        }

        GJBaseGameLayer::handleButton(down, button, isPlayer1);
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto* fields = m_fields.self();

        for (auto it = fields->pendingJumps.begin(); it != fields->pendingJumps.end();) {
            auto* player = it->player1 ? m_player1 : m_player2;

            if (!player) {
                it = fields->pendingJumps.erase(it);
                continue;
            }

            if (player->m_isOnGround && !player->m_isDart) {
                const bool wasReleased = it->released;
                const bool player1 = it->player1;

                fields->applyingBufferedInput = true;
                GJBaseGameLayer::handleButton(true, TICK_BUFFER_JUMP_BUTTON, player1);
                if (wasReleased) {
                    GJBaseGameLayer::handleButton(false, TICK_BUFFER_JUMP_BUTTON, player1);
                }
                fields->applyingBufferedInput = false;

                if (tickbuffer::verbose()) {
                    log::info("[Tick Buffer] applied buffered jump on landing");
                }
                it = fields->pendingJumps.erase(it);
                continue;
            }

            --it->remaining;
            if (it->remaining <= 0) {
                if (tickbuffer::verbose()) {
                    log::info("[Tick Buffer] pending jump expired");
                }
                it = fields->pendingJumps.erase(it);
            } else {
                ++it;
            }
        }

        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};

class $modify(TickBufferPlayLayer, PlayLayer) {
    void resetLevel() {
        auto* base = static_cast<TickBufferBaseLayer*>(
            static_cast<GJBaseGameLayer*>(this)
        );
        base->m_fields->pendingJumps.clear();
        PlayLayer::resetLevel();
    }
};

class $modify(TickBufferKeyboard, CCKeyboardDispatcher) {
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool repeat, double timestamp) {
        if (down && !repeat && key == KEY_F6) {
            tickbuffer::stage = (tickbuffer::stage + 1) % (tickbuffer::OFF + 1);
            tickbuffer::announceStage();
            return true;
        }

        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, repeat, timestamp);
    }
};

$on_mod(Loaded) {
    log::info(
        "\n[Tick Buffer] loaded for Geometry Dash 2.2081. "
        "F6 cycles 2 -> 4 -> 8 -> 16 -> 24 ticks -> OFF.\n"
    );
}
