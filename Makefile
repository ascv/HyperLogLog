build:
	python setup.py build
clean:
	python setup.py clean
	python3 setup.py clean
	python3.8 setup.py clean
	rm -r ./build/
	rm -r ./dist/
install:
	python3 setup.py install
	python setup.py install
remove:
	python3 setup.py uninstall HLL
