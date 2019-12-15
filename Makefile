build:
	python setup.py build
	python3 setup.py build
clean:
	python setup.py clean
	python3 setup.py clean
	rm -r ./build/
install:
	python setup.py install
	python3 setup.py install
remove:
	python setup.py uninstall HLL
	python3 setup.py uninstall HLL
