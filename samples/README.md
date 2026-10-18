# demo_app â€” the packaged example

A deliberately tiny 9-file project with the dependency shapes that make an
incremental builder interesting:

```
config.h â”€â”€â–¶ settings.h â”€â”€â–¶ main.cpp        (two-hop transitive chain)
user_model.h â”€â”€â–¶ home.cpp, user_model.cpp   (shared leaf dependency)
home.h â”€â”€â–¶ home.cpp, main.cpp               (header used only by includers)
```

Try it:

```sh
ripple init  samples/demo_app
# edit something, for example append a line to samples/demo_app/user_model.h
ripple status samples/demo_app
ripple what-if samples/demo_app config.h
ripple scan   samples/demo_app
```

Note that `main.cpp` is *not* affected when `user_model.h` changes
(it does not include it) but *is* affected when `config.h` changes (through
`settings.h`). That asymmetry is the analyzer doing its job.

`.ripple/baseline.bin` (created by `init`) holds the previous state; delete
the folder to start cold.
