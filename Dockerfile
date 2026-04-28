# KChess deployment image: builds the UCI engine then bundles the Node web bridge.
#
# Build context = the MyChess/ folder.
# Run example:  docker run --rm --network=host -e PORT=3049 \
#               -e LLM_BASE_URL=http://localhost:3073 kchess

FROM gcc:13-bookworm AS engine-build
WORKDIR /src
COPY algorithm/ ./algorithm/
COPY uci_main.cpp ./
RUN mkdir -p /out && \
    g++ -std=c++17 -O3 -march=x86-64-v3 -DNDEBUG \
        -static-libstdc++ -static-libgcc \
        -Ialgorithm/qt_compat -I. \
        algorithm/attack.cpp algorithm/board.cpp algorithm/define.cpp \
        algorithm/engine.cpp algorithm/evaluation.cpp algorithm/movegen.cpp \
        algorithm/tt.cpp algorithm/util.cpp \
        uci_main.cpp -pthread -o /out/MyChessUCI

FROM node:22-bookworm-slim
ENV NODE_ENV=production
WORKDIR /app

COPY web/package.json web/package-lock.json ./
RUN npm ci --omit=dev && npm cache clean --force

COPY web/server.js ./
COPY web/public/ ./public/
COPY --from=engine-build /out/MyChessUCI /app/engine/MyChessUCI

ENV MYCHESS_UCI_PATH=/app/engine/MyChessUCI \
    PORT=8080 \
    HOST=0.0.0.0 \
    LLM_BASE_URL=http://localhost:3073 \
    LLM_MODEL=qwen3.6-35b-a3b

EXPOSE 8080
CMD ["node", "server.js"]
