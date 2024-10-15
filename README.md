# Tree_Keeper

A tool that remembers file structure on external drives
Md5 algorythm borrowed from [https://github.com/Zunawe/md5-c](https://github.com/Zunawe/md5-c)

Made by:
- [nikhotmsk](https://tildegit.org/nikhotmsk)
- [penguin-operator](https://github.com/penguin-operator)

---

# Make.sh
This is an environment file, which acts like `venv/bin/activate` in python.
To activate it use
```shell
operator@penguin:treekeeper$ source make.sh
(make.sh) operator@penguin:treekeeper$ # or
(make.sh) operator@penguin:treekeeper$ . make.sh
(make.sh) operator@penguin:treekeeper$ build && run
```

After this you can use these commands:
- `build` yo build the project
- `run` to run the executable safely
- `envexit` to exit the venv
- `envreset` to restart the venv to see your customizations on it
- `help` to get short help for first 2 commands

Update: Commands are now executable without sourcing
```shell
operator@oenguin:treekeeper$ chmod +x make.sh
operator@oenguin:treekeeper$ ./make.sh build
operator@oenguin:treekeeper$ ./make.sh run --some --args
```