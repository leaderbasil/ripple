# demo_app — the packaged example

A deliberately tiny 9-file project with the dependency shapes that make an
incremental builder interesting:

```
config.h ──▶ settings.h ──▶ main.cpp        (two-hop transitive chain)
user_model.h ──▶ home.cpp, user_model.cpp   (shared leaf dependency)
home.h ──▶ home.cpp, main.cpp               (header used only by includers)
```

Try it:

```sh
algo1 init  samples/demo_app
# edit something, for example append a line to samples/demo_app/user_model.h
algo1 status samples/demo_app
algo1 what-if samples/demo_app config.h
algo1 scan   samples/demo_app
```

Note that `main.cpp` is *not* affected when `user_model.h` changes
(it does not include it) but *is* affected when `config.h` changes (through
`settings.h`). That asymmetry is the analyzer doing its job.

`.algo1/baseline.bin` (created by `init`) holds the previous state; delete
the folder to start cold.
