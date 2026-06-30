FROM debian:bookworm-slim AS build

RUN apt-get update \
  && apt-get install -y --no-install-recommends g++ make python3 libcurl4-openssl-dev ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN make clean && make check-cpp

FROM debian:bookworm-slim

RUN apt-get update \
  && apt-get install -y --no-install-recommends libcurl4 ca-certificates \
  && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY --from=build /app/build/retell_cerebras_v3 /app/retell_cerebras_v3

ENV PORT=8080
CMD ["/app/retell_cerebras_v3"]
