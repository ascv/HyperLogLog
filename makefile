BUILD_PATH = /home/josh/hll/build/lib.linux-x86_64-2.7

all:
	python setup.py build

clean:
	sudo rm -rf ./build
	sudo rm -f *~

auto: all
	cd /home/josh/hll/build/lib.linux-x86_64-2.7
	python

test:
	sudo ./test.sh

