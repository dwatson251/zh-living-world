# Living World server image: the Windows server programs (zh_host.exe + zh_client.exe) under Wine, headless. It contains NO game files:
# mount your own Zero Hour install at /data/zerohour. Build context = the unpacked release zip, i.e. a folder with LivingWorld/ in it
# (CI does this; see .github/workflows/release.yml).   docker build -f docker/Dockerfile -t living-world <context>
FROM debian:bookworm-slim AS wine
ARG WINE_VERSION=11.19
ARG WINE_SHA256=a1fe858b45d12c9b16b91c7601d9b934f68430f7b6b95e240212ab6e2a198d8a
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl xz-utils && rm -rf /var/lib/apt/lists/*
# portable "wow64" Wine build: runs 32-bit programs without any 32-bit system libraries
RUN curl -fsSL -o /wine.tar.xz "https://github.com/Kron4ek/Wine-Builds/releases/download/${WINE_VERSION}/wine-${WINE_VERSION}-amd64-wow64.tar.xz" \
 && echo "${WINE_SHA256}  /wine.tar.xz" | sha256sum -c - && mkdir /opt/wine && tar -xf /wine.tar.xz -C /opt/wine --strip-components=1

FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates curl tini xvfb xauth libfreetype6 libx11-6 libxext6 libxcursor1 libxrandr2 libxi6 libxrender1 libxfixes3 libxcomposite1 libxinerama1 libfontconfig1 fonts-liberation libudev1 libusb-1.0-0 libxkbcommon0 libxkbregistry0 libwayland-client0 libwayland-egl1 libpulse0 libasound2 libglib2.0-0 libgstreamer1.0-0 libgstreamer-plugins-base1.0-0 libpcsclite1 libpcap0.8 libgphoto2-6 libsane1 ocl-icd-libopencl1 libcapi20-3 \
 && rm -rf /var/lib/apt/lists/* && useradd -u 1000 -m lw
COPY --from=wine /opt/wine /opt/wine
# no Mono/Gecko: wineboot would open an installer dialog and try to download them
ENV PATH=/opt/wine/bin:$PATH WINEPREFIX=/data/world/.wineprefix WINEDEBUG=-all WINEDLLOVERRIDES="mscoree,mshtml="
COPY LivingWorld/zh_host.exe LivingWorld/zh_client.exe LivingWorld/um_server.ini /opt/livingworld/
COPY LivingWorld/cities/ /opt/livingworld/cities/
COPY docker/entrypoint.sh docker/healthcheck.sh /usr/local/bin/
RUN mkdir -p /data/world /data/zerohour /data/config && chown -R lw:lw /data
USER lw
VOLUME ["/data/world"]
EXPOSE 7777/tcp 8080/tcp
HEALTHCHECK --interval=30s --timeout=5s --start-period=480s --retries=3 CMD ["/usr/local/bin/healthcheck.sh"]
ENTRYPOINT ["/usr/bin/tini", "--", "/usr/local/bin/entrypoint.sh"]
