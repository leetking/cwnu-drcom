# fuck-drcom
针对西华师范大学做的一个校园网登录客户端，首要目标是为linux用户提供便利。

目前发现，我们学校好像只需要认证eap而且不需要心跳包就可以了。
因而这里就先实现eap认证就好了

## 使用
直接make就好了

1. **登录**

修改`drcomrc.example`里面的配置为自己的，并重命名为`drcomrc`放到当前目录下。
然后输入
```bash
sudo ./drcom
```
就可以实现登录了。
你也可以赋予drcom一个S权限并修改所有者为root
```bash
sudo chown root:root drcom; sudo chmod +s drcom
```
以后登录就直接`./drcom`就可以了

2. **注销**
```bash
sudo ./drcom -l
```

3. **重新登录**
```bash
sudo ./drocm -r
```

4. **帮助**
```bash
./drcom -h
```
