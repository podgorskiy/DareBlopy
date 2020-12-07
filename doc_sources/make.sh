rm -Rf build

sphinx-apidoc -o source/ ../_dareblopy
sphinx-build -M html source build

# cp -R build/html/* ../
