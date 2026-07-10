FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk

ENV AR=llvm-ar-11

WORKDIR /app
VOLUME /app/build

RUN apt update \
	&& apt install -y --no-install-recommends --no-install-suggests git python3-pip \
	&& apt autoremove -y \
	&& apt clean \
	&& rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/alliedmodders/ambuild
RUN pip install ./ambuild
RUN git config --global --add safe.directory /app

COPY . .
CMD [ "/bin/bash", "./docker-entrypoint.sh" ]
