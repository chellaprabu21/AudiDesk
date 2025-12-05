# AudiDeck Website Deployment Guide

## Step 1: Set Up Waitlist Collection

### Option A: Formspree (Recommended - 5 minutes)

1. Go to [formspree.io](https://formspree.io) and sign up (free)
2. Create a new form → Get your form ID (e.g., `xpznqwer`)
3. Edit `index.html` and replace:
   ```javascript
   const FORMSPREE_ID = 'YOUR_FORMSPREE_ID';
   ```
   with your actual ID:
   ```javascript
   const FORMSPREE_ID = 'xpznqwer';
   ```

**Features:**
- ✅ 50 free submissions/month
- ✅ Email notifications
- ✅ CSV export
- ✅ Spam filtering

---

### Option B: Google Sheets (Free, Unlimited)

1. Create a Google Sheet with columns: `email`, `timestamp`
2. Go to Extensions → Apps Script
3. Paste this code:

```javascript
function doPost(e) {
  const sheet = SpreadsheetApp.getActiveSpreadsheet().getActiveSheet();
  const data = JSON.parse(e.postData.contents);
  
  sheet.appendRow([
    data.email,
    new Date().toISOString(),
    data.source || 'Website'
  ]);
  
  return ContentService
    .createTextOutput(JSON.stringify({ success: true }))
    .setMimeType(ContentService.MimeType.JSON);
}
```

4. Deploy → New deployment → Web app
5. Set "Who has access" to "Anyone"
6. Copy the URL and update `index.html`:

```javascript
// Replace the Formspree fetch with:
const response = await fetch('YOUR_GOOGLE_SCRIPT_URL', {
    method: 'POST',
    body: JSON.stringify({ email: email })
});
```

---

## Step 2: Deploy the Website

### Option A: Vercel (Easiest - 2 minutes)

1. Install Vercel CLI:
   ```bash
   npm i -g vercel
   ```

2. Deploy:
   ```bash
   cd /Users/cv/go/src/github.com/Public/AudiDeck/website
   vercel
   ```

3. Follow prompts → Get URL like `audideck.vercel.app`

4. Add custom domain in Vercel dashboard (optional)

---

### Option B: GitHub Pages (Free)

1. Create repo on GitHub (e.g., `audideck-website`)

2. Push the website folder:
   ```bash
   cd /Users/cv/go/src/github.com/Public/AudiDeck/website
   git init
   git add .
   git commit -m "Initial commit"
   git branch -M main
   git remote add origin https://github.com/YOUR_USERNAME/audideck-website.git
   git push -u origin main
   ```

3. Go to repo Settings → Pages → Source: `main` branch

4. Your site will be at: `https://YOUR_USERNAME.github.io/audideck-website`

---

### Option C: Netlify (Free, Easy)

1. Go to [netlify.com](https://netlify.com)
2. Drag & drop the `website` folder onto the page
3. Done! Get URL like `audideck.netlify.app`

---

### Option D: Cloudflare Pages (Free, Fast)

1. Go to [pages.cloudflare.com](https://pages.cloudflare.com)
2. Connect GitHub repo or upload directly
3. Get URL + free SSL + global CDN

---

## Step 3: Custom Domain (Optional)

1. Buy domain (Namecheap, Cloudflare, Google Domains)
   - Suggested: `audideck.app`, `audideck.io`, `getaudideck.com`

2. Point DNS to your host:
   - **Vercel**: Add in dashboard, follow DNS instructions
   - **GitHub Pages**: Add CNAME file with your domain
   - **Netlify**: Add in dashboard, set DNS records

---

## Quick Deploy Commands

```bash
# Vercel (fastest)
cd website && npx vercel --prod

# Netlify
cd website && npx netlify deploy --prod --dir=.

# Surge.sh (simple)
cd website && npx surge . audideck.surge.sh
```

---

## Analytics (Optional)

Add before `</head>`:

```html
<!-- Plausible (privacy-friendly) -->
<script defer data-domain="audideck.app" src="https://plausible.io/js/script.js"></script>

<!-- Or Google Analytics -->
<script async src="https://www.googletagmanager.com/gtag/js?id=G-XXXXXXX"></script>
<script>
  window.dataLayer = window.dataLayer || [];
  function gtag(){dataLayer.push(arguments);}
  gtag('js', new Date());
  gtag('config', 'G-XXXXXXX');
</script>
```

---

## Checklist

- [ ] Set up Formspree/Google Sheets
- [ ] Update `FORMSPREE_ID` in index.html
- [ ] Test form submission locally
- [ ] Deploy to hosting provider
- [ ] (Optional) Add custom domain
- [ ] (Optional) Add analytics
- [ ] Share the link! 🚀

