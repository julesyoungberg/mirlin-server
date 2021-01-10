FROM mtgupf/essentia:ubuntu18.04-v2.1_beta5

RUN apt-get -y update && apt-get install -y

RUN apt-get -y install clang make cmake

COPY . /usr/app
WORKDIR /usr/app

RUN make server

EXPOSE 9002

CMD ["./server"]
