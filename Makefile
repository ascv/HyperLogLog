build:
	python3 setup.py build
clean:
	python3 setup.py clean
	rm -r ./dist/
	rm -r ./HLL.egg-info/
install:
	python3 setup.py install
remove:
	python3 setup.py uninstall HLL
sdist:
	python3 setup.py sdist
test_sdist:
	pip install .
upload:
	twine upload dist/*
