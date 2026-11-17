# NINMODS KERNEL — Feature & Task Tracking (task.md)

---

### Verified & Working (যা যা টেস্ট করা হয়েছে ও সম্পূর্ণ ঠিক আছে)
- esp line ok
- esp box ok
- esp health bar ok
- esp armor bar ok
- esp player name ok
- esp distance ok
- esp bot tag ok
- speed ok (safe 1.00x - 1.30x anti-ban)
- menu 1:1 aspect ratio ok (headerH == sidebarW)
- floating logo touch barrier ok

---

### In Progress / Under Testing (যেগুলো টেস্ট করা বাকি বা ফিক্স চলছে)
- aim fov circle (স্ক্রিনের মাঝে FOV সার্কেল রেন্ডারিং ফিক্স করা হয়েছে pewpew অনুযায়ী — টেস্ট বাকি)
- aim silent (০x৪০ রে-কাস্ট ডিরেকশন ফিক্স ও টগল লিকেজ ফিক্স — টেস্ট বাকি)
- aim bot (স্মুথ এইম অ্যাসিস্ট ক্যামেরা ট্র্যাকিং — টেস্ট বাকি)
- auto headshot (+০.২২মি. হেড পিভট বায়াস — টেস্ট বাকি)
- scope aim (স্কোপ অন হেডশট লক — টেস্ট বাকি)
- sniper headshot lock (স্নাইপার রাইফেলের জন্য ইনস্ট্যান্ট লক — টেস্ট বাকি)
- crosshair center (স্ক্রিন ক্রসহেয়ার — টেস্ট বাকি)
- loot esp (গ্রাউন্ড আইটেম ও উইপন স্ক্যানার — টেস্ট বাকি)
- wide fov / ipad view (ক্যামেরা ড্রোন ভিউ — টেস্ট বাকি)
- high jump (জাম্প হাইট মাল্টিপ্লায়ার — টেস্ট বাকি)
- auto-pickup range (দূরের আইটেম পিকআপ — টেস্ট বাকি)
- air flight (এলিভেশন মোড — টেস্ট বাকি)
- ghost hack (ঘোস্ট মোড — টেস্ট বাকি)
- phantom mode (ফ্যান্টম মোড — টেস্ট বাকি)
- hitbox scaler (এনিমি হিটবক্স এক্সপ্যান্ডার — টেস্ট বাকি)
- no recoil (রিকয়েল রিডাকশন — টেস্ট বাকি)
- no reload (ইনস্ট্যান্ট শট — টেস্ট বাকি)

---

### Log of Recent Fixes
1. **Aim FOV Circle Render Fix (pewpew model):**
   - Removed blockers (`inMatch` check and `enable/silent` check) that prevented the circle from appearing.
   - Switched from `GetBackgroundDrawList()` to `GetForegroundDrawList()` so overlay renders on top of game canvas.
   - Adjusted color and line thickness (crisp white 220 alpha + orchid glow) centered at `(sw * 0.5f, sh * 0.5f)`.
2. **Aim Silent & Server Kick Fix:**
   - Removed writes to `HitCollider 0x20`, `HitLocation 0x28`, `HitGroup 0x64` to prevent `"Match results is still being calculated"`.
   - Now writing strictly to `AimInfo + 0x40` raycast direction.
   - Added `clear_silent_data()` to prevent memory flooding when toggles are turned OFF.
