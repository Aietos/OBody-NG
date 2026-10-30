#pragma once

namespace Preview::Physics {
    // HDT-SMP simulates hair and clothes in the world, so an actor moved or turned while time is frozen leaves their
    // physics bones behind and stretches the meshes. Reset function rebuilds the physics even with game frozen.
    void Reset();
}  // namespace Preview::Physics
