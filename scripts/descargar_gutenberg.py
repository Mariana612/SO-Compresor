#!/usr/bin/env python3
"""Descarga los libros de texto plano del top 100 de Gutenberg de los ultimos 30 dias."""

from __future__ import annotations

import argparse
import re
import sys
import time
from html.parser import HTMLParser
from pathlib import Path
from urllib.error import HTTPError, URLError
from urllib.parse import urljoin
from urllib.request import Request, urlopen


RANKING_URL = "https://www.gutenberg.org/browse/scores/top"
DEFAULT_OUTPUT = Path(__file__).resolve().parent.parent / "data" / "gutenberg"
USER_AGENT = "gutenberg-top30-downloader/1.0"
BOOK_URL_RE = re.compile(r"^/ebooks/(\d+)(?:$|[?#])")


class RankingParser(HTMLParser):
    """Extrae los enlaces de libros del bloque books-last30."""

    def __init__(self) -> None:
        super().__init__()
        self.in_target = False
        self.target_heading_seen = False
        self.book_ids: list[int] = []

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        attributes = dict(attrs)
        if tag == "h2" and attributes.get("id") == "books-last30":
            self.target_heading_seen = True
            self.in_target = True
        elif tag == "h2" and self.in_target and self.target_heading_seen:
            self.in_target = False

        if self.in_target and tag == "a":
            match = BOOK_URL_RE.match(attributes.get("href", ""))
            if match:
                book_id = int(match.group(1))
                if book_id not in self.book_ids:
                    self.book_ids.append(book_id)


class PlainTextParser(HTMLParser):
    """Obtiene el primer enlace de texto plano UTF-8 de una pagina de libro."""

    def __init__(self) -> None:
        super().__init__()
        self._link_depth = 0
        self._text: list[str] = []
        self.plain_text_url: str | None = None

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        if self.plain_text_url or tag != "a":
            return
        attributes = dict(attrs)
        href = attributes.get("href", "")
        if "text/plain" in attributes.get("type", "").lower() or "Plain Text" in href:
            self._link_depth = 1
            self._text = []
            self._candidate_url = href

    def handle_data(self, data: str) -> None:
        if self._link_depth:
            self._text.append(data)

    def handle_endtag(self, tag: str) -> None:
        if tag == "a" and self._link_depth:
            label = " ".join("".join(self._text).split()).lower()
            if "plain text" in label or "text/plain" in label or ".txt" in self._candidate_url.lower():
                self.plain_text_url = self._candidate_url
            self._link_depth = 0


def fetch(url: str) -> bytes:
    request = Request(url, headers={"User-Agent": USER_AGENT})
    with urlopen(request, timeout=30) as response:
        return response.read()


def ranking_book_ids() -> list[int]:
    parser = RankingParser()
    parser.feed(fetch(RANKING_URL).decode("utf-8", errors="replace"))
    return parser.book_ids[:100]


def plain_text_url(book_id: int) -> str | None:
    parser = PlainTextParser()
    book_url = f"https://www.gutenberg.org/ebooks/{book_id}"
    parser.feed(fetch(book_url).decode("utf-8", errors="replace"))
    if parser.plain_text_url:
        return urljoin(book_url, parser.plain_text_url)
    return None


def filename_from_url(url: str, book_id: int) -> str:
    name = Path(url.split("?", 1)[0]).name
    if not name.lower().endswith(".txt"):
        name = f"pg{book_id}.txt"
    return re.sub(r"[^A-Za-z0-9._-]", "_", name)


def download_books(output_dir: Path, limit: int, delay: float) -> tuple[int, int]:
    output_dir.mkdir(parents=True, exist_ok=True)
    downloaded = skipped = 0
    for position, book_id in enumerate(ranking_book_ids()[:limit], start=1):
        try:
            text_url = plain_text_url(book_id)
            if not text_url:
                print(f"[{position}] {book_id}: sin formato de texto plano, omitido")
                skipped += 1
                continue
            destination = output_dir / filename_from_url(text_url, book_id)
            destination.write_bytes(fetch(text_url))
            print(f"[{position}] descargado: {destination.name}")
            downloaded += 1
            if delay:
                time.sleep(delay)
        except (HTTPError, URLError, TimeoutError, UnicodeError, OSError) as error:
            print(f"[{position}] {book_id}: error: {error}", file=sys.stderr)
            skipped += 1
    return downloaded, skipped


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-o", "--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("-n", "--limit", type=int, default=100)
    parser.add_argument("--delay", type=float, default=0.2, help="segundos entre descargas")
    args = parser.parse_args()
    if not 1 <= args.limit <= 100:
        parser.error("--limit debe estar entre 1 y 100")
    if args.delay < 0:
        parser.error("--delay no puede ser negativo")

    downloaded, skipped = download_books(args.output, args.limit, args.delay)
    print(f"Finalizado: {downloaded} descargados, {skipped} omitidos.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())