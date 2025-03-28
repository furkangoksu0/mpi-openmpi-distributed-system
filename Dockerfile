FROM ubuntu:20.04

# Zaman dilimi ayarı (kurulum sırasındaki etkileşimi önlemek için)
ENV TZ=Europe/Istanbul
RUN ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone

# Gerekli paketlerin kurulumu
RUN apt-get update && apt-get install -y \
    build-essential \
    mpich \
    libopenmpi-dev \
    wget \
    git

# Çalışma dizininin oluşturulması
WORKDIR /app

# Veri dosyası için dizin oluştur
RUN mkdir -p /app/src/dataset

# Kaynak kodların ve scriptlerin kopyalanması
COPY src/ /app/src/
COPY scripts/ /app/scripts/

# Script'e çalıştırma izni verme
RUN chmod +x /app/scripts/run.sh

# Derleme işlemi için dizin oluşturma
RUN mkdir -p /app/build

# Derleme işlemi
RUN mpic++ -fopenmp src/main.cpp -o main 