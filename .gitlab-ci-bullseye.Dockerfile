FROM igwn/base:bullseye

LABEL name="LALSuite Nightly - Debian Bullseye" \
      maintainer="Adam Mercer <adam.mercer@ligo.org>" \
      support="Not Supported"

# add debian packages to container
COPY debs /srv/local-apt-repository

# install debs & cleanup
RUN apt-get update && \
      apt-get -y upgrade && \
      apt-get -y install lalsuite lalsuite-dev lalsuite-octave && \
      dpkg -P lalsuite lalsuite-dev lalsuite-octave && \
      apt-get -y install local-apt-repository && \
      /usr/lib/local-apt-repository/rebuild && \
      apt-get update && \
      apt-get upgrade && \
      rm -rf /var/lib/apts/lists/*
