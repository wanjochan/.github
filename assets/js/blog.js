(() => {
  const POSTS_URL = (typeof window !== 'undefined' && window.POSTS_JSON_URL) || '/posts.json';
  const PAGE_SIZE = (typeof window !== 'undefined' && window.POSTS_PER_PAGE) || 6;
  const CATEGORY_FALLBACK = (typeof window !== 'undefined' && window.DEFAULT_CATEGORY_LABEL) || '';

  const listEl = document.getElementById('posts-list');
  const pagerEl = document.getElementById('pagination');
  if (!listEl || !pagerEl) return;

  const getPageFromURL = () => {
    const params = new URLSearchParams(window.location.search);
    const p = parseInt(params.get('page') || '1', 10);
    return Number.isFinite(p) && p > 0 ? p : 1;
  };

  const setPageInURL = (page) => {
    const url = new URL(window.location.href);
    url.searchParams.set('page', String(page));
    history.replaceState({}, '', url.toString());
  };

  const renderPosts = (posts, page) => {
    const start = (page - 1) * PAGE_SIZE;
    const slice = posts.slice(start, start + PAGE_SIZE);
    if (slice.length === 0) {
      listEl.innerHTML = '<p>No posts yet.</p>';
      pagerEl.innerHTML = '';
      return;
    }

    listEl.innerHTML = slice.map(p => {
      const cat = (p.category && String(p.category)) || CATEGORY_FALLBACK;
      const dateAttr = p.date || '';
      const datePretty = p.date_pretty || '';
      const excerpt = (p.excerpt || '').toString();
      const title = (p.title || '').toString();
      const url = (p.url || '#').toString();
      return `
        <article class="post-card">
          <div class="post-meta">
            <time datetime="${dateAttr}">${datePretty}</time>
            ${cat ? `<span class="post-category">${cat}</span>` : ''}
          </div>
          <h3 class="post-title"><a href="${url}">${title}</a></h3>
          <p class="post-excerpt">${excerpt}</p>
          <a class="read-more" href="${url}">Read More →</a>
        </article>
      `;
    }).join('');
  };

  const renderPager = (total, page) => {
    const pages = Math.ceil(total / PAGE_SIZE);
    if (pages <= 1) { pagerEl.innerHTML = ''; return; }
    const link = (n, label = n, disabled = false, active = false) => {
      const cls = [
        'page-link',
        disabled ? 'disabled' : '',
        active ? 'active' : ''
      ].filter(Boolean).join(' ');
      if (disabled) return `<span class="${cls}">${label}</span>`;
      const href = setParam(window.location.href, 'page', String(n));
      return `<a class="${cls}" href="${href}" data-page="${n}">${label}</a>`;
    };
    const items = [
      link(Math.max(1, page - 1), '‹ Prev', page === 1),
      ...range(1, pages).map(n => link(n, String(n), false, n === page)),
      link(Math.min(pages, page + 1), 'Next ›', page === pages)
    ];
    pagerEl.innerHTML = items.join('');
    pagerEl.querySelectorAll('a[data-page]').forEach(a => {
      a.addEventListener('click', (e) => {
        e.preventDefault();
        const n = parseInt(a.getAttribute('data-page') || '1', 10);
        setPageInURL(n);
        render(n);
      });
    });
  };

  const setParam = (url, key, value) => {
    const u = new URL(url, window.location.origin);
    u.searchParams.set(key, value);
    return u.toString();
  };
  const range = (start, end) => Array.from({ length: end - start + 1 }, (_, i) => start + i);

  let ALL = [];
  const render = (page) => {
    renderPosts(ALL, page);
    renderPager(ALL.length, page);
  };

  // Cache-busting query to avoid CDN/Pages caching
  const POSTS_URL_BUSTED = setParam(POSTS_URL, 't', String(Date.now()));
  fetch(POSTS_URL_BUSTED, { headers: { 'Accept': 'application/json' } })
    .then(r => r.json())
    .then(json => {
      ALL = Array.isArray(json) ? json : [];
      const page = getPageFromURL();
      render(page);
    })
    .catch(() => {
      listEl.innerHTML = '<p>Failed to load posts.</p>';
      pagerEl.innerHTML = '';
    });
})();
