#include "Papyrus/Papyrus.h"
#include "Papyrus/PapyrusBody.h"

namespace Papyrus {
    void Bind() {
        if (const auto* const papyrus{SKSE::GetPapyrusInterface()}) {
            papyrus->Register(PapyrusBody::Bind);
        }
    }
}  // namespace Papyrus