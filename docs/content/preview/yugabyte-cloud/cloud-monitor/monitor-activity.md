---
title: Monitor cluster activity
linkTitle: Cluster activity
description: View the activity on your cluster.
headcontent: View the activity on your cluster
image: /images/section_icons/deploy/enterprise.png
menu:
  preview_yugabyte-cloud:
    identifier: monitor-activity
    parent: cloud-monitor
    weight: 600
type: docs
---

Review activity on your cluster using the **Activity** tab. The tab lists cluster activity, including the activity type, status, and start and end time.

To review activity for your entire cloud, use the [**Activity** tab](../../cloud-secure-clusters/cloud-activity/) on the **Admin** page.

![Cluster Activity tab](/images/yb-cloud/cloud-clusters-activity.png)

To view cluster activity details, click the activity in the list to display the **Activity Details** sheet.

## Logged activity

The following table lists the cluster activity that is logged.

| Source | Activity |
| :----- | :------- |
| Cluster | Create cluster<br>Edit cluster<br>Upgrade cluster<br>Pause cluster<br>Resume cluster |
| Read Replica | Create read replica<br>Edit read replica<br>Delete read replica |
| Allow List | Edit IP Allow Lists |
<!-- | Backup | Create backup<br>Delete backup<br>Restore backup | -->
