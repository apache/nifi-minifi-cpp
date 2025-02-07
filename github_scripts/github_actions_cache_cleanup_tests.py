#!/bin/python3

import unittest
from unittest.mock import MagicMock
from github_actions_cache_cleanup import GithubActionsCacheCleaner


class TestGithubActionsCacheCleaner(unittest.TestCase):
    def create_mock_github_request_sender(self):
        mock = MagicMock()
        mock.list_open_pull_requests = MagicMock()
        open_pull_requests = [
            {
                "number": 227,
                "title": "MINIFICPP-13712 TEST1",
            },
            {
                "number": 228,
                "title": "MINIFICPP-9999 TEST2",
            },
            {
                "number": 229,
                "title": "MINIFICPP-123 TEST3",
            }
        ]
        mock.list_open_pull_requests.return_value = open_pull_requests
        caches = {
            "actions_caches": [
                {
                    "id": 999,
                    "key": "macos-xcode-ccache-refs/pull/226/merge-6c8d283f5bc894af8dfc295e5976a5f154753123",
                },
                {
                    "id": 11111,
                    "key": "ubuntu-24.04-ccache-refs/pull/227/merge-9d6d283f5bc894af8dfc295e5976a5f1b46649c4",
                },
                {
                    "id": 11112,
                    "key": "ubuntu-24.04-ccache-refs/pull/227/merge-1d6d283f5bc894af8dfc295e5976a5f154753487",
                },
                {
                    "id": 12345,
                    "key": "macos-xcode-ccache-refs/pull/227/merge-2d6d283f5bc894af8dfc295e5976a5f154753536",
                },
                {
                    "id": 22221,
                    "key": "macos-xcode-ccache-refs/heads/MINIFICPP-9999-9d5e183f5bc894af8dfc295e5976a5f1b4664456",
                },
                {
                    "id": 22222,
                    "key": "macos-xcode-ccache-refs/heads/MINIFICPP-9999-8f4d283f5bc894af8dfc295e5976a5f1b4664123",
                },
                {
                    "id": 44444,
                    "key": "ubuntu-24.04-all-clang-ccache-refs/heads/main-1d4d283f5bc894af8dfc295e5976a5f1b4664456",
                },
                {
                    "id": 55555,
                    "key": "ubuntu-24.04-all-clang-ccache-refs/heads/main-2f4d283f5bc894af8dfc295e5976a5f1b4664567",
                }
            ]
        }
        mock.list_caches = MagicMock()
        mock.list_caches.return_value = caches
        mock.delete_cache = MagicMock()
        return mock

    def create_empty_open_pr_mock_github_request_sender(self):
        mock = MagicMock()
        mock.list_open_pull_requests = MagicMock()
        mock.list_open_pull_requests.return_value = []
        caches = {
            "actions_caches": [
                {
                    "id": 999,
                    "key": "macos-xcode-ccache-refs/pull/226/merge-6c8d283f5bc894af8dfc295e5976a5f154753123",
                },
                {
                    "id": 11111,
                    "key": "ubuntu-24.04-ccache-refs/pull/227/merge-9d6d283f5bc894af8dfc295e5976a5f1b46649c4",
                },
                {
                    "id": 11112,
                    "key": "ubuntu-24.04-ccache-refs/pull/227/merge-1d6d283f5bc894af8dfc295e5976a5f154753487",
                },
                {
                    "id": 12345,
                    "key": "macos-xcode-ccache-refs/pull/227/merge-2d6d283f5bc894af8dfc295e5976a5f154753536",
                },
                {
                    "id": 22221,
                    "key": "macos-xcode-ccache-refs/heads/MINIFICPP-9999-9d5e183f5bc894af8dfc295e5976a5f1b4664456",
                },
                {
                    "id": 22222,
                    "key": "macos-xcode-ccache-refs/heads/MINIFICPP-9999-8f4d283f5bc894af8dfc295e5976a5f1b4664123",
                },
                {
                    "id": 44444,
                    "key": "ubuntu-24.04-all-clang-ccache-refs/heads/main-1d4d283f5bc894af8dfc295e5976a5f1b4664456",
                },
                {
                    "id": 55555,
                    "key": "ubuntu-24.04-all-clang-ccache-refs/heads/main-2f4d283f5bc894af8dfc295e5976a5f1b4664567",
                }
            ]
        }
        mock.list_caches = MagicMock()
        mock.list_caches.return_value = caches
        mock.delete_cache = MagicMock()
        return mock

    def create_empty_caches_mock_github_request_sender(self):
        mock = MagicMock()
        mock.list_open_pull_requests = MagicMock()
        open_pull_requests = [
            {
                "number": 227,
                "title": "MINIFICPP-13712 TEST1",
            },
            {
                "number": 228,
                "title": "MINIFICPP-9999 TEST2",
            },
            {
                "number": 229,
                "title": "MINIFICPP-123 TEST3",
            }
        ]
        mock.list_open_pull_requests.return_value = open_pull_requests
        mock.list_caches = MagicMock()
        caches = {
            "actions_caches": []
        }
        mock.list_caches.return_value = caches
        mock.delete_cache = MagicMock()
        return mock

    def test_cache_cleanup(self):
        cleaner = GithubActionsCacheCleaner("mytoken", "githubuser/nifi-minifi-cpp")
        cleaner.github_request_sender = self.create_mock_github_request_sender()
        cleaner.remove_obsolete_cache_entries()
        self.assertEqual(set([call[0][0] for call in cleaner.github_request_sender.delete_cache.call_args_list]),
                         {"macos-xcode-ccache-refs/pull/226/merge-6c8d283f5bc894af8dfc295e5976a5f154753123",
                          "macos-xcode-ccache-refs/heads/MINIFICPP-9999-9d5e183f5bc894af8dfc295e5976a5f1b4664456",
                          "ubuntu-24.04-ccache-refs/pull/227/merge-9d6d283f5bc894af8dfc295e5976a5f1b46649c4",
                          "ubuntu-24.04-all-clang-ccache-refs/heads/main-1d4d283f5bc894af8dfc295e5976a5f1b4664456"})

    def test_cache_cleanup_with_zero_open_prs(self):
        cleaner = GithubActionsCacheCleaner("mytoken", "githubuser/nifi-minifi-cpp")
        cleaner.github_request_sender = self.create_empty_open_pr_mock_github_request_sender()
        cleaner.remove_obsolete_cache_entries()
        self.assertEqual(set([call[0][0] for call in cleaner.github_request_sender.delete_cache.call_args_list]),
                         {"macos-xcode-ccache-refs/pull/226/merge-6c8d283f5bc894af8dfc295e5976a5f154753123",
                          "ubuntu-24.04-ccache-refs/pull/227/merge-9d6d283f5bc894af8dfc295e5976a5f1b46649c4",
                          "ubuntu-24.04-ccache-refs/pull/227/merge-1d6d283f5bc894af8dfc295e5976a5f154753487",
                          "macos-xcode-ccache-refs/pull/227/merge-2d6d283f5bc894af8dfc295e5976a5f154753536",
                          "macos-xcode-ccache-refs/heads/MINIFICPP-9999-9d5e183f5bc894af8dfc295e5976a5f1b4664456",
                          "ubuntu-24.04-all-clang-ccache-refs/heads/main-1d4d283f5bc894af8dfc295e5976a5f1b4664456"})

    def test_cache_cleanup_with_zero_action_caches(self):
        cleaner = GithubActionsCacheCleaner("mytoken", "githubuser/nifi-minifi-cpp")
        cleaner.github_request_sender = self.create_empty_caches_mock_github_request_sender()
        cleaner.remove_obsolete_cache_entries()
        self.assertEqual(set([call[0][0] for call in cleaner.github_request_sender.delete_cache.call_args_list]), set())


if __name__ == '__main__':
    unittest.main()
