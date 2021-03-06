
IP协议是TCP/IP协议族的动力，为上层提供无状态、无连接、不可靠的服务。

1. 无状态:通信双方不同步传输数据的状态信息，因此IP数据报的发送、传输和接收都是相互独立、没有上下文。
2. 无连接：通信双方不长久的维持对方的任何信息。
3. 不可靠:不能保证IP数据报准确到达接收端，只是最大努力交付。(TTL到期或者头部校验失败，都会返回ICMP错误消息)

### IPV4 头部结构

http://7ktumn.com1.z0.glb.clouddn.com/timg.jpeg

1. 4位版本号:指定IP协议版本号。IPv4值为4，其他ipv4协议有不同的版本号.
2. 4位头部长度，指示有多少个32bit,因此头部长度最大为15*4=60字节。
3. 8位TOS:服务类型。3位优先权(已经被忽略),4位服务类型(最小延时、最大吞吐量、最高可靠性、最小费用)，一位保留。
4. 16位总长度:整个ip数据报长度，因为长度超过MTU会分片，因此实际传输的远远没有达到最大值。
5. 16位标识:唯一标识主机发送的每个数据报，初始值由系统随机生成，每发送一个值加1，主要用来分片。
5. 3位标识: 第一位保留，第二位DF:是否禁止分片;第三位MF:除了最后一个，其他分片设置为1
6. 13位片偏移:用于分片，用于指示距离相对于原始IP数据报开始处偏移(相对于数据)
7. 8位生存时间TTL: 到达目的地之前允许经过的最大路由器跳数。减到0，路由器丢弃并发送ICMP差错报文。
8. 8位协议:用来区分上层协议。(ICMP 1 TCP 6 UDP 17)
9. 32位源IP,目的IP,用来标识发送端和接收端.
10. 最大40字节的变长option信息:
    1) 记录路由:所有路由器将字节的ip地址写入.
    2) 时间戳:告诉每个路由器将数据报转发的时间写入IP头部。
    3) 松散路由选择:指定一个路由器IP地址列表。
    4) 严格路由选择:只能经过指定的路由器。

```
-->$sudo tcpdump -ntx -i lo
IP 127.0.0.1.35756 > 127.0.0.1.telnet: Flags [S], seq 3962380668, win 65495, options [mss 65495,sackOK,TS val 238345857 ecr 0,nop,wscale 6], length 0
	0x0000:  4510 003c fffd 4000 4006 3cac 7f00 0001
	0x0010:  7f00 0001 8bac 0017 ec2d 217c 0000 0000
	0x0020:  a002 ffd7 c9df 0000 0204 ffd7 0402 080a
	0x0030:  0e34 de81 0000 0000 0103 0306
IP 127.0.0.1.telnet > 127.0.0.1.35756: Flags [R.], seq 0, ack 3962380669, win 0, length 0
	0x0000:  4510 0028 0000 4000 4006 3cbe 7f00 0001
	0x0010:  7f00 0001 0017 8bac 0000 0000 ec2d 217d
	0x0020:  5014 0000 1860 0000
```

### IP分片

1. 分片可能发生在发送端、终端路由器，而且可能传输过程中多次分片，但只有在目标机器上，才会重新组装。
2. 以太网帧MTU是1500（ifconfig,netstat查看）

```
$ping 10.187.160.110 -s 1473
$sudo tcpdump -ntv -i eth0 icmp
tcpdump: listening on eth0, link-type EN10MB (Ethernet), capture size 65535 bytes
IP (tos 0x0, ttl 64, id 45920, offset 0, flags [+], proto ICMP (1), length 1396)
    10.0.0.71 > 10.187.160.110: ICMP echo request, id 4153, seq 1, length 1376
IP (tos 0x0, ttl 64, id 45920, offset 1376, flags [none], proto ICMP (1), length 125)
    10.0.0.71 > 10.187.160.110: icmp
```
### IP路由

http://7ktumn.com1.z0.glb.clouddn.com/WechatIMG1.jpeg

#### 路由机制
TODO
#### 路由表更新
TODO
### IP转发
$ echo 1 > /proc/sys/net/ipv4/ip_forward

数据报转发子模块对期望转发的数据报执行操作:
1. 检查TTL,如果=0，丢弃
2. 检查头部的严格路由选项。如果设置，检查目标IP是否是本机ip,如果不是回复一个ICMP源站路由失败报文。
3. 如果必要，给源端发送一个ICMP重定向报文，告诉他更合适的下一跳路由。
4. TTL -1
5. 处理IP头部选项
6. 如果必要，执行分片操作。


### 重定向
