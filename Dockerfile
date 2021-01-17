FROM ubuntu:16.04
USER root

ENV ZCM_HOME /zcm

RUN apt-get update
RUN apt-get install --no-install-recommends -yq \
    sudo apt-utils ca-certificates python3 python*-distutils python3-pip && \
    update-alternatives --install /usr/bin/python python /usr/bin/python3 1 && \
    update-alternatives --install /usr/bin/pip pip /usr/bin/pip3 1

COPY ./scripts/install-deps.sh $ZCM_HOME/scripts/install-deps.sh
RUN bash -c '$ZCM_HOME/scripts/install-deps.sh'

COPY DEBIAN/ $ZCM_HOME/DEBIAN
COPY LICENSE $ZCM_HOME/LICENSE
COPY config/ $ZCM_HOME/config
COPY docs/ $ZCM_HOME/docs
COPY examples/ $ZCM_HOME/examples
COPY gen/ $ZCM_HOME/gen
COPY scripts/ $ZCM_HOME/scripts
COPY test/ $ZCM_HOME/test
COPY tools/ $ZCM_HOME/tools
COPY util/ $ZCM_HOME/util
COPY waf $ZCM_HOME/waf
COPY waftools/ $ZCM_HOME/waftools
COPY wscript $ZCM_HOME/wscript
COPY zcm/ $ZCM_HOME/zcm

RUN bash -c 'export JAVA_HOME=$(readlink -f /usr/bin/javac | sed "s:/bin/javac::") && \
             export PATH=$PATH:~/.local/bin && \
             export NVM_DIR=$HOME/.nvm && \
             [ -s "$NVM_DIR/nvm.sh" ] && \. "$NVM_DIR/nvm.sh" && \
             export PATH=$PATH:$ZCM_HOME/deps/julia/bin:$ZCM_HOME/deps/cxxtest/bin && \
             cd $ZCM_HOME && \
             ./waf distclean configure --use-all --use-dev && \
             ./waf build && \
             ./waf install && \
             ./waf build_examples'
RUN echo 'source /zcm/examples/env' >> /root/.bashrc
