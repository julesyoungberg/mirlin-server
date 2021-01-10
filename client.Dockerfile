FROM mtgupf/essentia:ubuntu18.04-v2.1_beta5

RUN apt-get -y update && apt-get install -y

RUN apt-get -y install clang make

COPY . /usr/src
WORKDIR /usr/src

RUN make client

CMD ["./mirlinclient"]
