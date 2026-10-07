# 版本变更 / Changelog

按日期和作者简述对用户可见的功能变化；详细实现历史见 Git。使用方法见 [README](../README.zh-CN.md)。
User-visible changes by date and author; Git retains implementation history. See [README](../README.md) for usage.

## 2026-10-07 · lf7817

- 阅读：进度行右端显示电量百分比；工具栏上沿在校时后显示日期与时间（ASCII 格式，如 10-07 Wed 21:40）加电量，未校时只显示电量、不显示假时间。
  Show battery percent at the right end of the reading progress line; the toolbar's top edge shows the date and time in ASCII (e.g. 10-07 Wed 21:40) plus battery once synced, and only battery before that.
- 阅读：工具栏状态行按最近翻页节奏显示本章剩余时间（需 ≥5 个有效样本、本章分页已排完且非末页；间隔 <1 秒或 >5 分钟的样本丢弃，开书清零）。设备只有逐章分页，故不显示全书剩余时间。
  Show the estimated time left in the current chapter on the toolbar status line, from recent page-turn pace (needs ≥5 valid samples, a fully paginated chapter and a non-final page; gaps under 1s or over 5min are dropped, reset on open). Only per-chapter, since the device paginates chapter by chapter.
- 睡眠：新增「自动锁屏 Auto lock」四档（关 / 5 / 10 / 30 分钟，默认关，不改变现有行为）；闲置超时与电源键短按共用同一条锁屏路径，菜单开着时先收菜单并重新计时。另加低电量自动锁屏：电量 ≤5% 且未充电时锁一次，插入充电器后重新允许。
  Add an Auto lock selector (off / 5 / 10 / 30 minutes, off by default so existing behaviour does not change); the idle timeout reuses the exact short-power-key lock path, and an open menu closes first instead of locking. Also lock once when the battery is at or below 5% and not charging, re-armed when a charger is attached.
- WiFi：传书页以已有 WiFi 取得地址后做一次 NTP 校时（pool.ntp.org，固定 UTC+8），同步完成即停止；不写 PMU RTC，断电后需重新校时。
  After the transfer page joins an existing WiFi and gets an address, sync the wall clock once via NTP (pool.ntp.org, fixed UTC+8) and stop; the PMU RTC is not written, so a power cycle needs another sync.

## 2026-09-28 · UNSaWEN

- 阅读：进入书架先确认是否续读；ZIP目录连续读取、字宽测量不生成位图，当前章先排起始/续读页，其余分页分批补齐。
  Confirm resume before opening the last book; read ZIP directories sequentially, measure advances without rasterizing, and paginate the initial/resume pages before completing the chapter incrementally.
- EPUB插图：默认占位，点击后单独预览本地JPEG/PNG，返回保持正文位置；标题前的章首图片及同资源单独标为标题图，其他重复图片提示本次已读章节中最早出现的位置，开章不再反复解码重复图片。
  Show image placeholders and load local JPEG/PNG previews only on tap, returning to the same text position; label opening title images and matching resources separately, retaining earliest-visited hints for other repeated illustrations and avoid decoding images on chapter opening.
- 手势：滑动翻页阈值由120缩短到64像素，保留抬手提交、轻点容差和短拖取消。
  Reduce swipe distance from 120 to 64 pixels while retaining release-to-commit, tap tolerance and short-drag cancellation.
- EPUB：ZIP 条目、资源项及章节上限提高到 32768，使用固定窗口扫描ZIP目录和紧凑索引查找，标题按实际长度保存，OPF/导航上限4 MiB；打开和排版显示等待提示，失败显示具体原因。
  Raise EPUB ZIP, manifest and spine limits to 32768, scan ZIP directories through a fixed window, use compact indexed lookup and actual-length titles, and allow 4 MiB OPF/navigation metadata; show parsing/layout wait hints and failure reasons.
- WiFi：TF 卡支持上传完整 TTF 字体，32 MiB 上限、同名确认、提交前结构校验及中断恢复；传书期间暂停卡上字体读取，离页恢复。
  Upload complete TTF fonts to TF over WiFi with a 32 MiB limit, confirmed replacement, pre-commit structural checks and interruption recovery; suspend card font reads during transfer and resume on exit.
- 部署：随仓库提供未修改的 ChillDuanSans VF v1.30、原 OFL-1.1 许可和来源校验；内置子集命名为 Read Pico UI，保留原版权与许可。
  Bundle unmodified ChillDuanSans VF v1.30, original OFL-1.1 terms and source verification; name the embedded subset Read Pico UI and retain attribution and licensing.

## 2026-09-27 · UNSaWEN

- EPUB 图片位置增加“[图片]”占位，避免纯图片章节显示为空白；暂不解码图片。
  Show “[图片]” placeholders at EPUB image positions so image-only chapters are visible; image decoding is not yet supported.

- 修复长篇 EPUB 因条目或章节超过 512 而无法打开的问题；上限提高到 2048，单资源解压上限仍为 2 MiB。
  Fix long EPUBs failing to open above 512 entries or chapters; raise the limit to 2048 while retaining the 2 MiB uncompressed resource limit.

- 图书阅读：支持 TF 卡或内置存储的 UTF-8/GBK TXT 与纯文本 EPUB，目录、逐书续读、字号、手势翻页及默认关闭的实验晃动翻页。
  Read UTF-8/GBK TXT and text-only EPUB from TF or internal storage, with a TOC, per-book resume, font sizes, gestures and optional experimental shake-to-turn.
- 图书管理：增加拼音/首字母/英文搜索、来源筛选、名称/最近阅读排序、详情，以及分别确认的单本/批量删除和清进度。
  Add pinyin/initials/English search, source filters, name/recent sorting, details and separately confirmed single/batch deletion or progress reset.
- WiFi 传书：支持设备热点和已有网络、触屏/网页配网、连接与网址二维码；网页管理当前存储，支持确认替换、取消和重试；停止后返回进入前的位置。
  Transfer over the device hotspot or an existing network, with touchscreen/web provisioning and connection/URL QR codes. Manage current storage in the browser, confirm replacements, cancel or retry uploads, and return to the entry location on stop.
- 可靠性：处理保存失败与部分成功重试、中文文件名和覆盖中断恢复；修复显示欠载后相位队列残留导致的死锁；检测 TF 失效后停止相关消费者并回退字体，插回后需显式重挂。
  Handle save failures, partial-operation retries, Chinese filenames and interrupted replacements. Fix a display queue deadlock after underrun; stop affected consumers and fall back fonts on TF loss, requiring explicit remount after reinsertion.
