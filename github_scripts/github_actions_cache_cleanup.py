#!/bin/python3

import requests
import re
import argparse
import logging
from typing import Dict, List


logging.basicConfig(level=logging.INFO)


class GithubRequestSender:
    def __init__(self, token: str, repository: str):
        self.headers = {
            'Accept': 'application/vnd.github+json',
            'Authorization': 'Bearer ' + token,
            'X-GitHub-Api-Version': '2022-11-28'
        }
        self.repository = repository

    def _send_get_request(self, url: str):
        response = requests.get(url, headers=self.headers)
        response.raise_for_status()

        return response.json()

    def _send_delete_request(self, url: str, params: Dict[str, str]):
        response = requests.delete(url, headers=self.headers, params=params)
        response.raise_for_status()

    def list_open_pull_requests(self):
        url = "https://api.github.com/repos/" + self.repository + "/pulls?state=open"
        return self._send_get_request(url)

    def list_caches(self):
        url = "https://api.github.com/repos/" + self.repository + "/actions/caches"
        return self._send_get_request(url)

    def delete_cache(self, key: str):
        url = "https://api.github.com/repos/" + self.repository + "/actions/caches"
        self._send_delete_request(url, params={"key": key})


class CacheEntry:
    def __init__(self, id: int, key: str):
        self.id = id
        self.key = key

    def __str__(self) -> str:
        return "{id: " + str(self.id) + ", key: " + self.key + "}"

    def __repr__(self):
        return str(self)


class GithubActionsCacheCleaner:
    def __init__(self, token: str, repository: str):
        self.github_request_sender = GithubRequestSender(token, repository)

    def _list_open_pr_ids(self) -> List[str]:
        open_tickets = []
        for request in self.github_request_sender.list_open_pull_requests():
            open_tickets.append(request['number'])
        return open_tickets

    def _get_cache_entries(self) -> List[CacheEntry]:
        entries = []
        json_result = self.github_request_sender.list_caches()
        for json_entry in json_result["actions_caches"]:
            entries.append(CacheEntry(int(json_entry["id"]), json_entry["key"]))
        return entries

    def _is_pr_already_closed(self, entry, open_tickets):
        match = re.search(r'refs/pull/([\d]+)/merge', entry.key)
        return match and int(match[1]) not in open_tickets

    def _remove_non_latest_branch_caches(self, entry: CacheEntry, latest_branch_cache_map: Dict[str, CacheEntry], removable_entries: List[str]):
        cache_mapping_key = "-".join(entry.key.split("-")[0:-1])
        if cache_mapping_key not in latest_branch_cache_map:
            latest_branch_cache_map[cache_mapping_key] = entry
        else:
            if latest_branch_cache_map[cache_mapping_key].id < entry.id:
                removable_entries.append(latest_branch_cache_map[cache_mapping_key].key)
                latest_branch_cache_map[cache_mapping_key] = entry
            else:
                removable_entries.append(entry.key)

    def _get_removable_cache_entries(self) -> List[str]:
        removable_entries = []
        latest_branch_cache_map = dict()
        open_prs = self._list_open_pr_ids()
        for entry in self._get_cache_entries():
            if self._is_pr_already_closed(entry, open_prs):
                removable_entries.append(entry.key)
                continue

            self._remove_non_latest_branch_caches(entry, latest_branch_cache_map, removable_entries)

        return removable_entries

    def _remove_cache_entries(self, entries_to_remove: List[str]):
        for key in entries_to_remove:
            logging.info('Removing cache entry: %s', key)
            self.github_request_sender.delete_cache(key)

    def remove_obsolete_cache_entries(self):
        self._remove_cache_entries(self._get_removable_cache_entries())


if __name__ == "__main__":
    parser = argparse.ArgumentParser(prog='Github Actions Cache Cleaner', description='Cleans up obsolete cache entries of github actions')
    parser.add_argument('-r', '--repository', help='The name of the repository in <owner>/<repository> format', required=True)
    parser.add_argument('-t', '--token', help='Github token for API access', required=True)
    args = parser.parse_args()
    cache_cleaner = GithubActionsCacheCleaner(args.token, args.repository)
    cache_cleaner.remove_obsolete_cache_entries()
