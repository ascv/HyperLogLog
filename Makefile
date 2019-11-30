build:
	python setup.py build
clean:
	python setup.py clean
	rm -r ./build/
install:
	python setup.py install
remove:
	python setup.py uninstall HLL
