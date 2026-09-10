FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk

ENV AR=llvm-ar-11

WORKDIR /app
VOLUME /app/build

RUN apt update -o Acquire::Check-Valid-Until=false \
	&& apt install -y --no-install-recommends --no-install-suggests git \
	&& apt autoremove -y \
	&& apt clean \
	&& rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/alliedmodders/ambuild
RUN cd ambuild && python3 setup.py install
RUN git config --global --add safe.directory /app

COPY . .
CMD [ "/bin/bash", "./docker-entrypoint.sh" ]
