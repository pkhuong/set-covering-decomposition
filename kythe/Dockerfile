FROM debian:buster-slim AS build

RUN apt-get update && apt-get install -y wget

RUN wget https://github.com/kythe/kythe/releases/download/v0.0.30/kythe-v0.0.30.tar.gz
RUN echo "d970e4678231dd038dbf6d7a4721adf7af0a3975783b4e6a1d702ed727867c89  kythe-v0.0.30.tar.gz" | sha256sum -c -
RUN tar xzf kythe-v0.0.30.tar.gz && mv kythe-v0.0.30 /opt/kythe

FROM debian:buster-slim
RUN apt-get update && \
    apt-get install -y libncurses5 parallel &&  \
    ((echo "will cite" | parallel --citation > /dev/null 2>&1) || true)
COPY --from=build /opt/kythe /opt/kythe/
