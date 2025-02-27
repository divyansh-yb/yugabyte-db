---
title: YugabyteDB Quick start for macOS
headerTitle: Quick start
linkTitle: Quick start
headcontent: Create a local cluster on a single host
description: Get started using YugabyteDB in less than five minutes on macOS.
aliases:
  - /preview/quick-start/create-local-cluster/
  - /preview/quick-start/install/
layout: single
type: docs
rightNav:
  hideH4: true
unversioned: true
---

<ul class="nav nav-tabs-alt nav-tabs-yb">
  <li>
    <a href="../quick-start-yugabytedb-managed/" class="nav-link">
      <img src="/icons/cloud.svg" alt="Cloud Icon">
      Use a cloud cluster
    </a>
  </li>
  <li class="active">
    <a href="../quick-start/" class="nav-link">
      <img src="/icons/database.svg" alt="Server Icon">
      Use a local cluster
    </a>
  </li>
</ul>

The local cluster setup on a single host is intended for development and learning. For production deployment, performance benchmarking, or deploying a true multi-node on multi-host setup, see [Deploy YugabyteDB](../deploy/).

<ul class="nav nav-tabs-alt nav-tabs-yb">
  <li class="active">
    <a href="../quick-start/" class="nav-link">
      <i class="fa-brands fa-apple" aria-hidden="true"></i>
      macOS
    </a>
  </li>
  <li>
    <a href="../quick-start/linux/" class="nav-link">
      <i class="fa-brands fa-linux" aria-hidden="true"></i>
      Linux
    </a>
  </li>
  <li>
    <a href="../quick-start/docker/" class="nav-link">
      <i class="fa-brands fa-docker" aria-hidden="true"></i>
      Docker
    </a>
  </li>
  <li>
    <a href="../quick-start/kubernetes/" class="nav-link">
      <i class="fa-regular fa-dharmachakra" aria-hidden="true"></i>
      Kubernetes
    </a>
  </li>
</ul>

## Install YugabyteDB

Installing YugabyteDB involves completing [prerequisites](#prerequisites) and [downloading the packaged database](#download-yugabytedb).

### Prerequisites

Before installing YugabyteDB, ensure that you have the following available:

- <i class="fa-brands fa-apple" aria-hidden="true"></i> macOS 10.12 or later.

- Python 3. To check the version, execute the following command:

    ```sh
    python --version
    ```

    ```output
    Python 3.7.3
    ```

- `wget` or `curl`.

    Note that the following instructions use the `wget` command to download files. If you prefer to use `curl` (included in macOS), you can replace `wget` with `curl -O`.

    To install `wget` on your Mac, you can run the following command if you use Homebrew:

    ```sh
    brew install wget
    ```

#### Set file limits

Because each tablet maps to its own file, you can create a very large number of files in the current shell by experimenting with several hundred tables and several tablets per table. Execute the following command to ensure that the limit is set to a large number:

```sh
launchctl limit
```

It is recommended to have at least the following soft and hard limits:

```output
maxproc     2500        2500
maxfiles    1048576     1048576
```

Edit `/etc/sysctl.conf`, if it exists, to include the following:

```sh
kern.maxfiles=1048576
kern.maxproc=2500
kern.maxprocperuid=2500
kern.maxfilesperproc=1048576
```

If this file does not exist, create the following two files:

- `/Library/LaunchDaemons/limit.maxfiles.plist` and insert the following:

  ```xml
  <?xml version="1.0" encoding="UTF-8"?>
  <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
  <plist version="1.0">
    <dict>
      <key>Label</key>
        <string>limit.maxfiles</string>
      <key>ProgramArguments</key>
        <array>
          <string>launchctl</string>
          <string>limit</string>
          <string>maxfiles</string>
          <string>1048576</string>
          <string>1048576</string>
        </array>
      <key>RunAtLoad</key>
        <true/>
      <key>ServiceIPC</key>
        <false/>
    </dict>
  </plist>
  ```

- `/Library/LaunchDaemons/limit.maxproc.plist` and insert the following:

  ```xml
  <?xml version="1.0" encoding="UTF-8"?>
  <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
  <plist version="1.0">
    <dict>
      <key>Label</key>
        <string>limit.maxproc</string>
      <key>ProgramArguments</key>
        <array>
          <string>launchctl</string>
          <string>limit</string>
          <string>maxproc</string>
          <string>2500</string>
          <string>2500</string>
        </array>
      <key>RunAtLoad</key>
        <true/>
      <key>ServiceIPC</key>
        <false/>
    </dict>
  </plist>
  ```

Ensure that the `plist` files are owned by `root:wheel` and have permissions `-rw-r--r--`. To take effect, you need to reboot your computer or run the following commands:

  ```sh
sudo launchctl load -w /Library/LaunchDaemons/limit.maxfiles.plist
sudo launchctl load -w /Library/LaunchDaemons/limit.maxproc.plist
  ```

You might need to `unload` the service before loading it.

### Download YugabyteDB

You download YugabyteDB as follows:

1. Download the YugabyteDB `tar.gz` file by executing the following `wget` command:

    ```sh
    wget https://downloads.yugabyte.com/releases/{{< yb-version version="preview">}}/yugabyte-{{< yb-version version="preview" format="build">}}-darwin-x86_64.tar.gz
    ```

1. Extract the package and then change directories to the YugabyteDB home, as follows:

    ```sh
    tar xvfz yugabyte-{{< yb-version version="preview" format="build">}}-darwin-x86_64.tar.gz && cd yugabyte-{{< yb-version version="preview">}}/
    ```

## Create a local cluster

{{< tabpane text=true >}}

  {{% tab header="macOS Pre-Monterey" lang="Pre-Monterey" %}}

On macOS pre-Monterey, create a single-node local cluster with a replication factor (RF) of 1 by running the following command:

```sh
./bin/yugabyted start
```

  {{% /tab %}}

  {{% tab header="macOS Monterey" lang="Monterey" %}}

macOS Monterey enables AirPlay receiving by default, which listens on port 7000. This conflicts with YugabyteDB and causes `yugabyted start` to fail. Use the [--master_webserver_port flag](../reference/configuration/yugabyted/#advanced-flags) when you start the cluster to change the default port number, as follows:

```sh
./bin/yugabyted start --master_webserver_port=9999
```

Alternatively, you can disable AirPlay receiving, then start YugabyteDB normally, and then, optionally, re-enable AirPlay receiving.

  {{% /tab %}}

{{< /tabpane >}}

### Check the cluster status

Execute the following command to check the cluster status:

```sh
./bin/yugabyted status
```

Expect an output similar to the following:

```output
+--------------------------------------------------------------------------------------------------+
|                                            yugabyted                                             |
+--------------------------------------------------------------------------------------------------+
| Status              : Running.                                                                   |
| Replication Factor  : 1                                                                          |
| Web console         : http://127.0.0.1:7000                                                      |
| JDBC                : jdbc:postgresql://127.0.0.1:5433/yugabyte?user=yugabyte&password=yugabyte  |
| YSQL                : bin/ysqlsh   -U yugabyte -d yugabyte                                       |
| YCQL                : bin/ycqlsh   -u cassandra                                                  |
| Data Dir            : /Users/myuser/var/data                                                     |
| Log Dir             : /Users/myuser/var/logs                                                     |
| Universe UUID       : fad6c687-e1dc-4dfd-af4b-380021e19be3                                       |
+--------------------------------------------------------------------------------------------------+
```

After the cluster has been created, clients can [connect to the YSQL and YCQL APIs](#connect-to-the-database) at `http://localhost:5433` and `http://localhost:9042` respectively. You can also check `~/var/data` to see the data directory and `~/var/logs` to see the logs directory.

If you have previously installed YugabyteDB version 2.8 or later and created a cluster on the same computer, you may need to [upgrade the YSQL system catalog](../manage/upgrade-deployment/#upgrade-the-ysql-system-catalog) to run the latest features.

### Use the Admin UI

The cluster you have created consists of two processes: [YB-Master](../architecture/concepts/yb-master/) which keeps track of various metadata (list of tables, users, roles, permissions, and so on) and [YB-TServer](../architecture/concepts/yb-tserver/) which is responsible for the actual end-user requests for data updates and queries.

Each of the processes exposes its own Admin UI that can be used to check the status of the corresponding process, as well as perform certain administrative operations. The [YB-Master Admin UI](../reference/configuration/yb-master/#admin-ui) is available at [http://127.0.0.1:7000](http://127.0.0.1:7000) (replace the port number if you've started `yugabyted` with the `--master_webserver_port` flag) and the [YB-TServer Admin UI](../reference/configuration/yb-tserver/#admin-ui) is available at [http://127.0.0.1:9000](http://127.0.0.1:9000).

#### Overview and YB-Master status

The following illustration shows the YB-Master home page with a cluster with a replication factor of 1, a single node, and no tables. The YugabyteDB version is also displayed.

![master-home](/images/admin/master-home-binary-rf1.png)

The **Masters** section displays the 1 YB-Master along with its corresponding cloud, region, and zone placement.

#### YB-TServer status

Click **See all nodes** to open the **Tablet Servers** page that lists the YB-TServer along with the time since it last connected to the YB-Master using regular heartbeats. Because there are no user tables, **User Tablet-Peers / Leaders** is 0. As tables are added, new tablets (also known as shards) will be created automatically and distributed evenly across all the available tablet servers.

![master-home](/images/admin/master-tservers-list-binary-rf1.png)

## Connect to the database

Using the YugabyteDB SQL shell, [ysqlsh](../admin/ysqlsh/), you can connect to your cluster and interact with it using distributed SQL. ysqlsh is installed with YugabyteDB and is located in the bin directory of the YugabyteDB home directory.

To open the YSQL shell, run `ysqlsh`.

```sh
./bin/ysqlsh
```

```output
ysqlsh (11.2-YB-2.1.0.0-b0)
Type "help" for help.

yugabyte=#
```

To load sample data and explore an example using ysqlsh, refer to [Retail Analytics](../sample-data/retail-analytics/).

## Build an application

Applications connect to and interact with YugabyteDB using API client libraries (also known as client drivers). This section shows how to connect applications to your cluster using your favorite programming language.

### Choose your language

{{< readfile "/preview/quick-start-yugabytedb-managed/quick-start-buildapps-include.md" >}}

## Next step

[Explore YugabyteDB](../explore/)
