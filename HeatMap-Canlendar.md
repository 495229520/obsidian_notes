---
tags:
  - 模版
  - obsidian
  - dataview
---

# HeatMap-Canlendar

> [!note]
> 这个模板用于配合 `heatmap-calendar-obsidian` 和 `Dataview` 插件，按“每天修改过多少篇笔记”来生成活动热力图。
> 如果你想统计别的数据，只需要修改下面 `entries.push` 里的 `intensity` 和筛选条件。

```dataviewjs
dv.span("## 活动热力图")

const excludedFolders = ["模版", "图片", "剪切板"];
const pages = dv.pages("")
    .where(p => p.file && p.file.path)
    .where(p => !excludedFolders.some(folder => p.file.path.startsWith(folder + "/")));

const activityByDay = new Map();

for (const page of pages) {
    const day = page.file.mday?.toFormat("yyyy-MM-dd");
    if (!day) continue;

    activityByDay.set(day, (activityByDay.get(day) ?? 0) + 1);
}

const calendarData = {
    year: Number(dv.current().file.name.match(/\d{4}/)?.[0] ?? window.moment().format("YYYY")),
    colors: {
        green: ["#e8f5e9", "#b7e1b9", "#7bc67e", "#43a047", "#1b5e20"]
    },
    showCurrentDayBorder: true,
    defaultEntryIntensity: 1,
    entries: []
};

for (const [date, count] of activityByDay.entries()) {
    calendarData.entries.push({
        date,
        intensity: count,
        content: `${count}`,
        color: "green"
    });
}

renderHeatmapCalendar(this.container, calendarData);
```

## 说明

- 统计口径：当天有多少篇笔记发生了修改。
- 已排除目录：`模版/`、`图片/`、`剪切板/`。
- 如果你想看“创建活跃度”，把 `page.file.mday` 改成 `page.file.cday`。
- 如果你想只统计某个目录，比如 `1.C++基础/`，把 `dv.pages("")` 改成 `dv.pages('"1.C++基础"')`。
