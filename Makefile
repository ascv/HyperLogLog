build:
	python setup.py build
	python3 setup.py build
	python3.8 setup.py build
clean:
	python setup.py clean
	python3 setup.py clean
	python3.8 setup.py clean
	rm -r ./build/
	rm -r ./dist/
install:
	python setup.py install
	python3 setup.py install
	python3.8 setup.py install
remove:
	python setup.py uninstall HLL
	python3 setup.py uninstall HLL
	python3.8 setup.py uninstall HLL
sdist:
	python3 setup.py sdist
upload:
	python3 -m twine upload dist/*
small:
	python tests/small.py
	python3 tests/small.py
	python3.8 tests/small.py
memtest:
	python tests/memtest.py
	python3.8 tests/memtest.py
example:
	python tests/example.py
	python3.8 tests/example.py
