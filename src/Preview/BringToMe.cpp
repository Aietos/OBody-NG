#include "Preview/Preview.h"

// A copy of the previewed actor is shown where the player stands, for NPCs in cramped or dark places.
// The actor stays where they are, hidden with the other actors, so whatever they're doing isn't disturbed

namespace Preview {
    namespace {
        // Both in the same interior, or in the same worldspace outside
        bool IsInPlayersSpace(const RE::Actor* a_actor, const RE::PlayerCharacter* a_player) {
            const auto* const worldspace{a_actor->GetWorldspace()};
            if (worldspace != a_player->GetWorldspace()) return false;
            return worldspace || a_actor->GetParentCell() == a_player->GetParentCell();
        }
    }  // namespace

    bool Session::CanBringToPlayer() const {
        std::lock_guard lock{lock_};

        const auto* const player{RE::PlayerCharacter::GetSingleton()};
        // Mounted, the copy would stand in the player's place
        return target_ && player && target_.get() != player && !player->IsOnMount() &&
               IsInPlayersSpace(target_.get(), player) && copyFailedFor_ != target_->GetFormID();
    }

    bool Session::IsBroughtToPlayer() const {
        std::lock_guard lock{lock_};
        return bring_;
    }

    void Session::SetBroughtToPlayer(const bool a_brought) {
        std::lock_guard lock{lock_};
        if (a_brought == bring_ || (a_brought && !CanBringToPlayer())) return;

        // The copy is spawned there, moved there, or dropped if only Bring to me wanted it
        bring_ = a_brought;
        UpdateCopy();
    }
}  // namespace Preview
