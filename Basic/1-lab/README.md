# Лабораторная 1. Базовый трек

В качестве предметной области для работы мною была выбрана простая программа на языке c++ исходный код которой лежит в файле [main.cpp](main.cpp)

```cpp
// main.cpp

#include <chrono>
#include <iostream>
#include <thread>

int main() {
    std::cout << "C++ Worker is successfully started!" << std::endl;

    for (std::size_t i = 0; i < 3; ++i) {
        std::cout << "Working processing data..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return EXIT_SUCCESS;
}
```
## Часть 1: Работа с Dockerfile

### Разбор "плохого" Dockerfile и исправление ошибок

```Docker
# Dockerfile.bad

FROM gcc:latest
RUN apt-get update
RUN apt-get install -y cmake nano
COPY . /app
WORKDIR /app
RUN g++ -o worker main.cpp
CMD ["./worker"]
```

```Docker
# Dockerfile.good

FROM gcc:12.2-bullseye AS builder
WORKDIR /app
COPY main.cpp .
RUN g++ -O3 -o worker main.cpp

FROM debian:bullseye-slim
RUN useradd -m appuser
WORKDIR /app

COPY --from=builder /app/worker .
RUN chown appuser:appuser worker
USER appuser
CMD ["./worker"]
```


В файле [Dockerfile.bad](Dockerfile.bad) допущены следующие плохие практики:

**1. Использование тега `:latest` для базового образа**

**Почему это плохо:** тег `latest` указывает на последнюю собранную версию образа. При следующей сборке может скачаться совершенно другая мажорная версия компилятора (например, переход с GCC 11 на GCC 12), что сломает обратную совместимость или приведет к непредсказуемым ошибкам в рантайме.

**Как исправить:** в [Dockerfile.good](Dockerfile.good) зафиксирована версия образа: `gcc:12.2-bullseye`. Это гарантирует идемпотентность сборки на любой машине.

**2. Ошибки при работе с пакетным менеджером (`apt-get`)**
В плохом файле используются две подряд идущие команды `RUN`: `RUN apt-get update` и `RUN apt-get install -y cmake nano`. Это нарушает сразу целый комплекс правил:
1. **Нарушение Hadolint:** [DL3059](https://github.com/hadolint/hadolint/wiki/DL3059) — *Multiple consecutive RUN instructions. Consider consolidation.* (Каждый `RUN` создает новый слой в образе, увеличивая его размер).
2. **Нарушение Hadolint:** [DL3009](https://github.com/hadolint/hadolint/wiki/DL3009) — *Delete the apt-get lists after installing something.* (Оставшиеся списки пакетов занимают лишнее место).
3. **Нарушение Hadolint:** [DL3015](https://github.com/hadolint/hadolint/wiki/DL3015) — *Avoid additional packages by specifying `--no-install-recommends`.*
  
**Как исправить:** в хорошем Dockerfile мы вообще избавились от необходимости устанавливать эти пакеты в финальный образ. Сборка происходит в тяжелом слое `builder`, а в легковесный финальный образ `debian:bullseye-slim` копируется только готовый бинарный файл без лишних утилит и кэшей `apt`.

**3. Выполнение приложения от имени пользователя `root`**

**Почему это плохо:** по умолчанию все процессы в контейнере запускаются от имени суперпользователя. Если в C++ коде есть уязвимость (например, переполнение буфера), злоумышленник, эксплуатирующий её, получит права `root` внутри контейнера. Это риск безопасности, упрощающий атаку на систему.

**Как исправить:** в `Dockerfile.good` перед запуском приложения создается непривилегированный пользователь `appuser`, которому передаются права только на исполняемый файл. Директива `USER appuser` переключает контекст, и процесс работает с минимально необходимыми правами.

### Плохие практики работы с контейнерами

Даже с идеальным Dockerfile можно совершить ошибки при эксплуатации:

**1. Хранение данных в контейнере** — контейнеры временны. Если приложение пишет логи или сохраняет файлы прямо в файловую систему контейнера, то при его пересоздании данные будут потеряны. Правильная практика — использовать Docker Volumes для хранения состояний.

**2. Отношение к контейнеру как к виртуальной машине** — вмешательство в запущенный контейнер напрямую ломает инвариант контейнера. Любые изменения должны вноситься в `Dockerfile` с последующей пересборкой образа.

## Часть 2: Работа с Docker Compose

Для демонстрации хороших и плохих практик я добавил в docker compose систему управления баз данных PostgreSQL.

### Разбор "плохого" Docker Compose файла

В файле [docker-compose.bad.yml](docker-compose.bad.yml) допущены следующие ошибки:


```yml
# docker-composer.bad.yml

version: '3.8'
services:
  worker:
    build:
      context: .
      dockerfile: Dockerfile.bad
    
  database:
    image: postgres:latest
    environment:
      - POSTGRES_USER=admin
      - POSTGRES_PASSWORD=SuperSecretPassword123
    ports:
      - "5432:5432"
```

```yml
# docker-composer.good.yml

services:
  worker:
    build:
      context: .
      dockerfile: Dockerfile.good
    restart: unless-stopped
    networks:
      - worker_net

  database:
    image: postgres:15-alpine
    env_file:
      - .env
    restart: unless-stopped
    volumes:
      - db_data:/var/lib/postgresql/data
    networks:
      - db_net

volumes:
  db_data:

networks:
  worker_net:
    driver: bridge
  db_net:
    driver: bridge
```


**1. Хранение паролей в открытом виде прямо в коде.**

**Почему плохо:** Файл compose обычно коммитится в систему контроля версий (Git). Любой, кто получит доступ к репозиторию, увидит логины и пароли к базе данных.

**Как исправить:** В `docker-compose.good.yml` используется директива `env_file: - .env`. Сам файл `.env` добавляется в `.gitignore`.

*Примечание*: у меня .env запушен, но не обращайте на это внимание — это сделано намеренно, так как проект учебный =)

**2. Открытие портов базы данных наружу (Public Port Binding).**

**Почему плохо:** Директива `ports: - "5432:5432"` открывает порт PostgreSQL для всей хост-машины (а иногда и для всего интернета). Для общения сервисов внутри Docker-сети порты публиковать не нужно.

**Как исправить:** В хорошем файле убрана директива `ports`. Контейнеры внутри одной сети и так могут общаться друг с другом по стандартному порту через внутренний DNS Docker'а (по имени `database`).

**3. Отсутствие Volume для хранения данных БД.**

**Почему плохо:** При выполнении команды `docker-compose down` контейнер БД будет удален, а вместе с ним и все таблицы с данными.
  
**Как исправить:** В хорошем файле добавлен именованный том `db_data:/var/lib/postgresql/data`. Теперь данные персистентны и переживут перезапуск стека.

### Настройка изоляции по сети

В задании требовалось, чтобы контейнеры поднимались одним compose-файлом, но **не видели друг друга по сети**. 

Как это реализовано в `docker-compose.good.yml`: мы объявили две независимые пользовательские сети в блоке `networks`: `worker_net` и `db_net`. 
Затем мы привязали сервис `worker` только к сети `worker_net`, а сервис `database` — только к сети `db_net`.

**Принцип работы такой изоляции:**

По умолчанию Docker Compose создает одну общую сеть для всех сервисов в файле, позволяя им свободно "пинговать" друг друга по именам контейнеров. 
Явно распределяя сервисы по разным custom-сетям, мы даем команду Docker'у создать разные сетевые мосты. На уровне ядра Linux (через `iptables` и сетевые пространства имен - `net namespaces`) Docker автоматически прописывает правила маршрутизации, которые запрещают пакетам переходить из одного виртуального моста в другой. В результате контейнеры изолированы на сетевом уровне (L2/L3), хотя технически работают на одном хосте и управляются одним процессом.