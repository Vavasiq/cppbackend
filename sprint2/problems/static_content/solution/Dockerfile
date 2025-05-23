# Для сборки образа из данного докер файла нужно применить команду
# docker build -t my_http_server .
# После этого запустить контейнер
# docker run --rm -p 80:8080 my_http_server

# ------------------------------------------------------------------------------ #
# Создать образ на основе базового слоя gcc (там будет ОС и сам компилятор).
# 11.3 — используемая версия gcc.
FROM gcc:11.3 as build

# Выполнить установку зависимостей внутри контейнера.
RUN apt update && \
    apt install -y \
      python3-pip \
      cmake \
    && \
    pip3 install conan==1.*

# копируем conanfile.txt в контейнер, профиль default.txt и запускаем conan install
COPY conanfile.txt /app/
COPY default.txt /root/.conan/profiles/
RUN mkdir /app/build && cd /app/build && \
    conan install .. --build=missing

# только после этого копируем остальные иходники
COPY ./src /app/src
COPY CMakeLists.txt /app/

RUN cd /app/build && \
    cmake -DCMAKE_BUILD_TYPE=Release .. && \
    cmake --build . 

# ------------------------------------------------------------------------------ #
FROM ubuntu:22.04 as run

# Создадим пользователя www
RUN groupadd -r www && useradd -r -g www www
USER www

# Скопируем приложение со сборочного контейнера в директорию /app.
# Не забываем также папку data, она пригодится.
COPY --from=build /app/build/bin/game_server /app/
COPY ./data /app/data

# Запускаем игровой сервер
ENTRYPOINT ["/app/game_server", "/app/data/config.json"]