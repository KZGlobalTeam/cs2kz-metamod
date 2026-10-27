FROM registry.gitlab.steamos.cloud/steamrt/sniper/sdk

ENV AR=llvm-ar-11
ENV CC=clang-22
ENV CXX=clang++-22

WORKDIR /app
VOLUME /app/build

RUN wget -4 --retry-on-host-error -O /etc/apt/keyrings/apt.llvm.org.asc https://apt.llvm.org/llvm-snapshot.gpg.key \
	&& echo "deb [signed-by=/etc/apt/keyrings/apt.llvm.org.asc] http://apt.llvm.org/bullseye/ llvm-toolchain-bullseye-22 main" > /etc/apt/sources.list.d/llvm.list \
	&& apt update \
	&& apt install -y --no-install-recommends --no-install-suggests clang-22 lld-22 \
	&& apt autoremove -y \
	&& apt clean \
	&& rm -rf /var/lib/apt/lists/*
RUN git clone https://github.com/alliedmodders/ambuild
RUN cd ambuild && python3 setup.py install
RUN git config --global --add safe.directory /app

COPY . .
CMD [ "/bin/bash", "./docker-entrypoint.sh" ]
