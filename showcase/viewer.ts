import type { Manifest, ImageEntry } from './src/types'

function formatBytes(bytes: number): string {
  if (bytes < 1024) return `${bytes} B`
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`
}

function formatRatio(ratio: number): { text: string; cls: string } {
  const pct = (1 - ratio) * 100
  if (pct > 0) return { text: `${pct.toFixed(1)}% smaller`, cls: 'stat-highlight' }
  if (pct < 0) return { text: `${Math.abs(pct).toFixed(1)}% larger`, cls: 'stat-negative' }
  return { text: 'same size', cls: '' }
}

function mimeToLabel(mime: string): string {
  return mime.replace('image/', '').toUpperCase()
}

function renderCard(entry: ImageEntry): HTMLElement {
  const card = document.createElement('div')
  card.className = 'card'
  card.dataset.mime = entry.sourceMime
  card.dataset.tags = entry.tags.join(',')

  const tagsHtml = entry.tags.map((t) => `<span class="tag">${t}</span>`).join('')

  const header = document.createElement('div')
  header.className = 'card-header'
  header.innerHTML = `
    <span>${entry.sourceFile} ${tagsHtml}</span>
    <span class="size">${formatBytes(entry.sourceSize)}</span>
  `
  card.appendChild(header)

  const body = document.createElement('div')
  body.className = 'card-body'

  // Original column
  const origCol = document.createElement('div')
  origCol.className = 'image-col'
  origCol.innerHTML = `
    <div class="label">Original</div>
    <img src="/images/${entry.originalImage}" alt="Original">
    <div class="stats">${formatBytes(entry.sourceSize)}</div>
  `
  body.appendChild(origCol)

  // Conversion columns
  for (const conv of entry.conversions) {
    const col = document.createElement('div')
    col.className = 'image-col'

    if (conv.error) {
      col.innerHTML = `
        <div class="label">${conv.function}</div>
        <div class="error-msg">Error: ${conv.error}</div>
      `
    } else {
      const ratio = formatRatio(conv.compressionRatio)
      col.innerHTML = `
        <div class="label">${conv.function} &rarr; ${mimeToLabel(conv.outputMime)}</div>
        <img src="/images/${conv.outputFile}" alt="${conv.function}">
        <div class="stats">
          ${formatBytes(conv.outputSize)}<br>
          <span class="${ratio.cls}">${ratio.text}</span><br>
          ${conv.durationMs} ms
        </div>
      `
    }
    body.appendChild(col)
  }

  card.appendChild(body)
  return card
}

async function main() {
  const res = await fetch('/manifest.json')
  const manifest: Manifest = await res.json()

  const meta = document.getElementById('meta')!
  meta.textContent = `Generated: ${new Date(manifest.generatedAt).toLocaleString()} | Platform: ${manifest.platform} | ${manifest.entries.length} images`

  const gallery = document.getElementById('gallery')!
  const filtersNav = document.getElementById('filters')!

  // Collect unique tags across all entries
  const allTags = [...new Set(manifest.entries.flatMap((e) => e.tags))].sort()
  const filters = ['All', ...allTags]

  for (const f of filters) {
    const btn = document.createElement('button')
    btn.className = `filter-btn${f === 'All' ? ' active' : ''}`
    btn.textContent = f
    btn.dataset.filter = f
    btn.addEventListener('click', () => {
      filtersNav.querySelectorAll('.filter-btn').forEach((b) => b.classList.remove('active'))
      btn.classList.add('active')
      applyFilter(f)
    })
    filtersNav.appendChild(btn)
  }

  for (const entry of manifest.entries) {
    gallery.appendChild(renderCard(entry))
  }

  function applyFilter(tag: string) {
    const cards = gallery.querySelectorAll<HTMLElement>('.card')
    for (const card of cards) {
      const tags = (card.dataset.tags || '').split(',')
      card.style.display = tag === 'All' || tags.includes(tag) ? '' : 'none'
    }
  }
}

main()
