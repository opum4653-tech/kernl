# NINMODS KERNEL — Feature & Task Tracking (task.md)

This document tracks all features, options, and operational statuses available in the NINMODS Kernel menu UI.

---

## 1. Visuals & ESP (ট্যাব ০: ভিজ্যুয়াল)

- [x] **ESP Laser Line** — `OK` (স্ক্রিন/টপ/বটম থেকে এনিমির দিকে লেজার ট্র্রেসার লাইন, স্ন্যাপ অ্যাঙ্কর সহ)
- [x] **ESP Box (Chaturbhuj)** — `OK` (Corner, Full, Glow - ৩ ধরণের ডায়নামিক বক্স স্টাইল)
- [x] **ESP Health Bar** — `OK` (ডানপাশে কালার কোডেড হেলথ বার: সবুজ/হলুদ/লাল)
- [x] **ESP Armor Bar** — `OK` (ভেস্ট/আর্মর স্ট্যাটাস বার)
- [x] **ESP Player Name** — `OK` (প্লেয়ার রিয়েল নেম টেক্সট রেন্ডারার)
- [x] **ESP Distance** — `OK` (মিটার অনুযায়ী রিয়েল-টাইম দূরত্ব প্রদর্শন)
- [x] **ESP Bot Tag (AI)** — `OK` (বট এবং আসল প্লেয়ার আলাদা করার ট্যাগ)
- [x] **ESP Weapon Name** — `OK` (শত্রুর হাতের অস্ত্রের নাম প্রদর্শন)
- [x] **Firing Indicator** — `OK` (এনিমি গুলি চালানো শুরু করলে ভিজ্যুয়াল ফ্ল্যাশ অ্যালার্ট)
- [x] **Crosshair Center** — `OK` (স্ক্রিনের মাঝে প্রিসিশন ক্রসহেয়ার)
- [x] **Loot ESP** — `OK` (কাছের অস্ত্র ও গুরুত্বপূর্ণ লুটের আইটেম ডিসপ্লে)
- [x] **Ignore Bot Players** — `OK` (বট প্লেয়ারদের ESP থেকে ফিল্টার আউট করা)
- [x] **Max Render Distance Slider** — `OK` (৫০ মি. থেকে ৫০০ মি. রেঞ্জ কন্ট্রোল)
- [x] **Laser Line Width Slider** — `OK` (লাইন থিকনেস কন্ট্রোল ০.৫ - ৪.০ px)
- [x] **Box Border Width Slider** — `OK` (বক্স বর্ডার থিকনেস কন্ট্রোল ১.০ - ৫.০ px)
- [x] **ESP Accent Color Palette** — `OK` (৬টি থিম সোয়াচ কালার সিলেকশন)

---

## 2. Aimbot & Combat (ট্যাব ১: এইমবট ও কমব্যাট)

- [x] **Aim Silent** — `OK` (পিওর কার্নেল রে-কাস্ট ডিরেকশন ০x৪০ স্টিয়ারিং, ক্যামেরা না ঘুরিয়ে বুলেট সোজা টার্গেটে যায়, কোনো সার্ভার ডিসিঙ্ক/কিক নেই)
- [x] **Auto Headshot (100% Red)** — `OK` (+০.২২মি. এলিভেশন বায়াস দিয়ে হেড স্ফিয়ার কোলাইডারে ১০০% লাল নাম্বারে ড্যামেজ)
- [x] **Enable Aim Assist** — `OK` (স্মুথ ক্যামেরা ট্র্যাকিং এইম অ্যাসিস্ট)
- [x] **Scope Aim (Instant Head)** — `OK` (স্কোপ অন করলেই তাত্ক্ষণিক হেড লক)
- [x] **Sniper Headshot Lock** — `OK` (AWM, Kar98, M82B ইত্যাদিতে অটো হেডশট লক)
- [x] **Target Aim Bone Selector** — `OK` (Head, Chest, Hip বোন সিলেকশন)
- [x] **Aim Trigger Condition** — `OK` (On Firing vs In FOV ট্রিগার মোড)
- [x] **Target Priority** — `OK` (Crosshair Distance, Lowest HP, 3D Distance মোড)
- [x] **Draw FOV Circle** — `OK` (স্ক্রিনে এইম এফওভি সার্কেল ড্রয়িং)
- [x] **Aim FOV Range Slider** — `OK` (১০° থেকে ৩৬০° ডায়নামিক এফওভি রেডিয়াস)
- [x] **Silent Aim Hit Rate Slider** — `OK` (০% থেকে ১০০% আরএনজি হিট রেট কন্ট্রোল)
- [x] **Headshot Elevation Bias Slider** — `OK` (০.১০মি. - ০.৩৫মি. রেঞ্জে ফাইন-টিউনিং, ডিফল্ট +০.২২মি. হেড সেন্টার)
- [x] **Aim Smooth Factor Slider** — `OK` (১.০ থেকে ২৫.০ স্মুথনেস কন্ট্রোল)
- [x] **Max Aiming Range Slider** — `OK` (২০মি. থেকে ৩০০মি. এইম রেঞ্জ)
- [x] **Safety Filters (Ignore Knocked, Ignore Bots, Visible Check)** — `OK` (নকড প্লেয়ার, বট এবং ওয়াল বিহাইন্ড ফিল্টারিং)
- [x] **Live Target HUD Telemetry** — `OK` (টার্গেটের নাম, হেলথ ও দূরত্বের লাইভ ডিসপ্লে)

---

## 3. Motion & Weapon Mods (ট্যাব ২: মোশন ও স্পিড)

- [x] **Speed Hack (Anti-Ban Safe)** — `OK` (RunSpeedUpScale 0x2C0 এর মাধ্যমে কেলি/অলক স্কিল রেঞ্জের নিরাপদ স্পিড, ১.০০x - ১.৩০x ক্ল্যাম্পড, স্মার্ট ফায়ারিং গার্ড সহ)
- [x] **Speed Multiplier Slider** — `OK` (১.০০x থেকে ১.৩০x ফাইন স্পিড কন্ট্রোল)
- [x] **Wide FOV (iPad View)** — `OK` (ক্যামেরা ড্রোন/আইপ্যাড ভিউ ১.০x - ২.৫x স্কেলার)
- [x] **High Jump** — `OK` (১.০x - ৫.০x জাম্প হাইট কন্ট্রোল)
- [x] **Auto-Pickup Range** — `OK` (৫মি. - ৩৫মি. দূর থেকে অটো লুট পিকআপ)
- [x] **Air Flight (Elevation)** — `OK` (২.০মি. - ১৫.০মি. ভার্টিক্যাল এলিভেশন মোড)
- [x] **Ghost Hack** — `OK` (ঘোস্ট মোড এন্ট্রি পয়েন্ট)
- [x] **Phantom Mode** — `OK` (ফ্যান্টম মোড ট্রান্সপারেন্সি)
- [x] **Hitbox Scaler** — `OK` (১.০x - ৩.০x হিটবক্স এক্সপ্যান্ডার)
- [x] **No Recoil** — `OK` (অস্ত্রের রিকয়েল রিডাকশন)
- [x] **No Reload** — `OK` (ইনস্ট্যান্ট শট / জিরো রিলোড ওয়েট)

---

## 4. UI & System Settings (ট্যাব ৩: সিস্টেম ও মেনু)

- [x] **Master Switch** — `OK` (এক ক্লিকে সমস্ত ফিচার বন্ধ বা চালু করার গ্লোবাল সুইচ)
- [x] **Menu Aspect Ratio 1:1** — `OK` (সাইডবার প্রস্থ এবং হেডার উচ্চতা সমান ১:১ অনুপাত)
- [x] **Floating Logo Drag & Touch Barrier** — `OK` (৩৮.০ফ্লোট রেডিয়াসের ফ্লোটিং 'N' লোগো, ফুল স্ক্রিন টাচ প্রটেকশন সহ)
- [x] **Menu Opacity Slider** — `OK` (০.৪০ থেকে ১.০০ ট্রান্সপারেন্সি স্লাইডার)
- [x] **UI Scale Adjuster** — `OK` (হাই-ডিপিআই স্ক্রিনের জন্য অটো স্কেলিং)
- [x] **Theme Presets** — `OK` (Electric Orchid, Cyber Neon, Blood Crimson, Emerald Matrix, Royal Gold, Ice Frost)
- [x] **Daemon Status & Latency Monitor** — `OK` (কার্নেল ড্রাইভ কানেক্টিভিটি ও পিং স্ট্যাটাস)
