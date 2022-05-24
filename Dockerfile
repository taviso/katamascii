FROM ubuntu:focal as builder

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y build-essential libglib2.0-dev chipmunk-dev libncurses-dev libcaca-dev \ 
    && apt-get clean && rm -rf /var/lib/apt/lists/

COPY . /opt/katamascii/

RUN cd /opt/katamascii \
    && make

FROM ubuntu:focal

LABEL description="A wrapper container for playing katamascii"

ARG UID=1000

RUN addgroup --system katamascii \
    && useradd -m -g katamascii -u ${UID} katamascii

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y libglib2.0-dev chipmunk-dev libncurses-dev libcaca-dev locales \ 
    && apt-get clean && rm -rf /var/lib/apt/lists/

RUN echo "en_US.UTF-8 UTF-8" >> /etc/locale.gen \
    && locale-gen

ENV LC_ALL=en_US.UTF-8

COPY --from=builder /opt/katamascii/art /home/katamascii/art
COPY --from=builder /opt/katamascii/tiles /home/katamascii/tiles
COPY --from=builder /opt/katamascii/katamascii /usr/local/bin/katamascii

USER katamascii
WORKDIR /home/katamascii

CMD [ "/usr/local/bin/katamascii" ]
