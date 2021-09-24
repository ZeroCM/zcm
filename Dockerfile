FROM ubuntu:16.04
USER root

ENV ZCM_HOME /zcm
WORKDIR $ZCM_HOME

RUN apt-get update && apt-get install -y sudo apt-utils

COPY ./scripts/install-deps.sh ./scripts/install-deps.sh

RUN bash -c './scripts/install-deps.sh -i -s'

COPY DEBIAN/ ./DEBIAN
COPY LICENSE ./LICENSE
COPY config/ ./config
COPY docs/ ./docs
COPY examples/ ./examples
COPY gen/ ./gen
COPY scripts/ ./scripts
COPY test/ ./test
COPY tools/ ./tools
COPY util/ ./util
COPY waf ./waf
COPY waftools/ ./waftools
COPY wscript ./wscript
COPY zcm/ ./zcm

ENV PYTHON /usr/bin/python3
ENV PATH ${PATH}:/root/.local/bin:$ZCM_HOME/deps/julia/bin
ENV NVM_DIR /root/.nvm

RUN bash -c 'export JAVA_HOME=$(readlink -f /usr/bin/javac | sed "s:/bin/javac::") && \
             [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" && \
             ./waf distclean configure --use-all --use-dev && \
             ./waf build && \
             ./waf install && \
             ./waf build_examples'

CMD bash -c 'export JAVA_HOME=$(readlink -f /usr/bin/javac | sed "s:/bin/javac::") && \
             [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" && \
             ./test/ci.sh'
