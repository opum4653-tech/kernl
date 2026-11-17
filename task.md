# NINMODS KERNEL — Active Feature Tracking (task.md)

---

### Core Active Features (একমাত্র সক্রিয় ১৬টি ফিচার)

#### Tab 0: Visuals (ESP)
1. **Master Switch** — পুরো সিস্টেম চালু/বন্ধ করার মেইন সুইচ [OK]
2. **ESP Box (Chaturbhuj)** — এনিমির চারপাশে 2D বাউন্ডিং বক্স [OK]
3. **ESP Armor Bar** — এনিমির ভেস্ট/আর্মর স্ট্যাটাস বার [OK]
4. **ESP Laser Line** — এনিমির দিকে লেজার ট্রেসার লাইন [OK]
5. **ESP Bot Tag (AI)** — সার্ভার বট প্লেয়ার আলাদা করার ট্যাগ [BOT] [OK]
6. **Max Render Distance Slider** — ইএসপি রেন্ডার রেঞ্জ (৫০ মি. - ৫০০ মি.) [OK]
7. **ESP Accent Color** — পার্পল কালার সিস্টেমে ফিক্সড (#E10098, মেনু কালার প্যালেট অপশন রিমুভড) [OK]

#### Tab 1: Combat (Aimbot)
8. **Aim Silent** — স্ক্রিন না ঘুরিয়েই সরাসরি বুলেটের ডিরেকশন হেডে নেওয়ার অপশন (Unnormalized Vector) [OK]
9. **Scope Aim (Instant Head)** — স্কোপ অন করলেই ইনস্ট্যান্ট হেড লক [OK]
10. **Auto Headshot (100% Red)** — ১০০% লাল নাম্বারে ড্যামেজ লক (LockedAimingCollider 0x80 সোয়াপ) [OK]
11. **Ignore Knocked** — নকড প্লেয়ারদের এইম ফিল্টার করা [OK]
12. **Draw FOV Circle** — স্ক্রিনের মাঝখানে কিল জোন সার্কেল আঁকা [OK]
13. **Headshot Elevation Bias Slider** — হেডে হিট লাগার উচ্চতা অ্যাডজাস্টমেন্ট (০.১০মি. - ০.৩৫মি., ডিফল্ট ০.২২মি.) [OK]

#### Tab 2: Mods & System
14. **Speed Hack (Anti-Ban Safe)** — নিরাপদ স্প্রিন্ট স্পিড বুস্ট (১.০০x - ১.৩০x) [OK]
15. **No Recoil** — বন্দুকের কাঁপুনি সম্পূর্ণ ০ করা [OK]
16. **EXIT OVERLAY Button** — কার্নেল ওভারলে সম্পূর্ণ বন্ধ করে দেওয়ার লাল বাটন [OK]

---

### Removed & Pruned Features
- Health Bar, Player Name, Distance, Weapon Name, Firing Indicator, Ignore Bots, Crosshair Center, Loot ESP, Box Style selector, Laser Line Width slider, Box Border Width slider.
- Aim Assist camera drag, Sniper Headshot Lock, Target Aim Bone selector, Aim Trigger selector, Target Priority selector, Visible Check, Silent Aim Hit Rate slider, Aim Smooth Factor slider, Max Aiming Range slider, Live Target HUD.
- Wide FOV/iPad View, High Jump, Auto-Pickup Range, Air Flight, Ghost Hack, Phantom Mode, Movement Diagnostics HUD, Hitbox Scaler, No Reload.
- **Backup Location:** All pruned offsets, structs, and feature snippets are safely archived outside the repository in `<appDataDir>/brain/<conversation-id>/backup_removed_offsets_and_features.md`.
