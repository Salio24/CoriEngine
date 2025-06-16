
auto player = ActiveScene->CreateEntity("Player");
auto rb = player.AddComponent<Rigidbody_EntityComponent>(Dynamic, {origin});

rb.AddShape()


