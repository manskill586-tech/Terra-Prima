# Terra Prima (MVP Skeleton)

Этот репозиторий содержит минимальный каркас движка и симуляции для планетарного LOD-мира.

## Сборка

```bash
cmake -S . -B build
cmake --build build
```

Запуск демо:

```bash
./build/terra_demo
```

Тесты:

```bash
ctest --test-dir build
```

## Структура

- `include/terra` — публичные интерфейсы
- `src` — реализация ядра
- `tests` — минимальные тесты планировщика и чанков

## Статус

MVP-каркас: LOD-планировщик, хранение чанков, сим-тайминг, Job System, снапшоты (заглушка), демо-цикл.



cmake -S genesis_engine -B genesis_engine/build
cmake --build genesis_engine/build --config Release
genesis_engine\build\Release\genesis_sim.exe
genesis_engine\build\Release\shm_reader.exe