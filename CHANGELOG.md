## [1.0] - 2026-07-10

### 🚀 Features

- Add facing left / facing right ASCII arts
- Atualização do Readme: Como executar, estrutura do projeto e decisões técnicas
- Add CLI

### 🐛 Bug Fixes

- Remove non ASCII characters
- Pequenas correções da estrutura do projeto
- *(clock)* Remove sleep while mutex is locked
- *(clock)* Add sleep outside critical section

### 🚜 Refactor

- *(simulation)* Cleanup simulation initialization

### 📚 Documentation

- Revise docstrings
- Revise `README` for clarity and updated instructions
- Revise vehicle request validation in README
## [0.3] - 2026-07-10

### 🚀 Features

- *(clock)* Synchronize analyser buffers on tick change and update thread args
- *(analyser)* Implement `analyser_get_previous_requests`
- *(analyser)* Add `analyser_get_status`
- *(clock)* Add `sleep(1)` between ticks
- Add ASCII arts
- *(traffic_light)* Implement `traffic_light_update`
- *(render)* Show priority row/collumn
- *(simulation)* Initialize traffic light
- *(traffic_light)* Implement double buffer to render previous state
- *(render)* Implement traffic light rendering logic
- *(simulation)* Initialize light assets
- *(map)* Introduce tile types to enable simulation decision-making
- *(vehicle)* Add 50% chance to turn at turn-designated tiles

### 🐛 Bug Fixes

- *(analyser)* Add foward ´Clock´ declaration
- *(clock)* Remove 'args->' from logs
- *(render)* Correct ASCII sprite alignment logic
- *(clock)* Replace `sleep` with `nanosleep` for high-resolution timing
- *(simulation)* Use correct asset for 'vehicle-medium'
- *(map)* Add better distribution for spawn points
- *(analyser)* Remove `analyser_get_status` declaration
- *(simulation)* Use correct asset for 'vehicle-medium'
- *(map)* Add better distribution for spawn points
- *(vehicle)* Remove unused internal function
- *(traffic_light)* Add NULL check
- *(vehicle)* Add NULL check
- *(simulation)* Pass parameter `trafic_light` to `vehicle_update`
- *(render)* Adjust priority displayment
- *(clock)* Swap traffic light buffers
- *(analyser)* Add missing function declaration
- Adjust tile rendering

### 💼 Other

- Add `_POSIX_C_SOURCE=200809L` compilation definition to `CMakeLists.txt`

### 🚜 Refactor

- *(clock)* [**breaking**] Decouple clock from vehicles and generalize to worker threads
- *(analyser)* Implement double buffering and fix array dimensions
- *(clock)* Remove mutex lock on `clock_get_tick`
- *(render)* Simplify rendering logic
- *(debug)* Move log to `out/debug.log`
- *(simulation)* Adapt simulation initialization
- *(vehicle)* Remove unused internal function

### 📚 Documentation

- *(traffic_light)* Update docstrings
## [0.2] - 2026-07-06

### 🚀 Features

- Initial project structure
- *(debug)* Create debug utilities
- *(clock)* Create clock workflow
- Create and destroy cars
- Adicionado regras de movimentação de veículos
- Implementado a logica de movimmentação de veículos
- *(map)* Implement dynamic file parsing and coarse-grained locking
- *(map)* Check for `TILE_BLOCKED` in `map_is_occupied` function
- *(vehicle)* Extract movement logic into `find_next_position` function
- *(vehicle)* Simplify `has_vehicle_ahead` logic using `find_next_position`
- *(debug)* Add variadic logging macros and update `TRY` macro
- Update project source files to use new logging macros
- *(traffic_light)* Add function signatures and documentation to header file
- *(vehicle)* Add `vehicle_get_priority_coord` signature and documentation
- Adiciona estrutura inicial do módulo traffic_light
- *(analyser)* Create analyser
- *(vehicle)* Create VehicleArgs to pass arguments to threads
- *(vehicle)* Implement `vehicle_update`
- *(render)* Add function signatures and documentation to header file
- *(render)* Add function implementations to source file
- Add macros to denote enum item counts
- *(render)* Implement render thread function
- *(simulation)* Implement core simulation orchestrator module
- Adicionado funções para pegar a posição, tipo e direção de um veiculo

### 🐛 Bug Fixes

- Remove redundant executables
- *(vehicle_movement)* Use correct include headers
- *(vehicle_movement)* [**breaking**] Switch function and variable identifiers to snake_case
- *(vehicle)* Remove redundant include for `debug.h`
- *(map)* Add bound checking on `map_get_tile_type`
- *(clock)* Add mutex lock to `clock_get_tick` to prevent race conditions
- *(clock)* Remove `const` qualifier from `clock_get_tick` signature
- *(debug)* Align macros in release mode
- Adicionado struct TrafficLight acima de sua definição para o compilador o reconhecer na hora de compilar
- *(vehicle)* Guard against assigning `DIRECTION_NONE` to vehicle direction
- *(vehicle)* Revert unintended code reindentation in vehicle module

### 💼 Other

- Configure `CMakeLists.txt`
- Implement deadlock prevention logic
- Enable automatic reconfiguration for GLOB sources
- Improve CMake configuration
- Correção do Deadlock.c e .h
- Validação do movimento do veículo no analyser

### 🚜 Refactor

- Adapt `vehicle` and `map` logic
- *(vehicle)* [**breaking**] Unify `vehicle` and `vehicle_movement` modules
- *(map)* Redesign public API using opaque pointers and value semantics
- *(vehicle)* Decouple concurrency management and position logic
- *(vehicle)* Change function name `update_position` to `try_update_position`

### 📚 Documentation

- Add contribution guidelines to `CONTRIBUTING.md`
- Update docstrings
- *(clock)* Add internal docstrings
- *(debug)* Update docstrings

### 🎨 Styling

- *(traffic_light)* Fix indentation
- *(analyser)* Format `analyser.c` to use Egyptian bracket style

### ⚙️ Miscellaneous Tasks

- Add build files, IDEs, and local environment to `.gitignore`
- Format `README` description for better readability
- Revise project title in `README`
- Resolve merge conflicts
