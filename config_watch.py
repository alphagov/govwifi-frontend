#!/usr/bin/env python3

import os
import subprocess
import sys
import time
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler

WATCH_DIR = '/etc/raddb'
TRIGGER_EVENTS = ('modified', 'deleted', 'moved', 'created')
IGNORE_EXTS = ('.swp',)
IGNORE_SUBDIRS = ('.idea', '.vscode')

def get_ext(path):
    return os.path.splitext(path)[-1]


def is_temp_file(path):
    if get_ext(path) in IGNORE_EXTS:
        return True
    if path[-1] == '~':
        return True
    return False


def is_valid_event(event):
    if event.is_directory:
        return False
    if event.event_type not in TRIGGER_EVENTS:
        return False
    if is_temp_file(event.src_path):
        return False
    for sub_dir in IGNORE_SUBDIRS:
        if sub_dir in event.src_path:
            return False
    return True


class ConfigChangeHandler(FileSystemEventHandler):

    @staticmethod
    def on_any_event(event):
        if is_valid_event(event):
            print('Config watcher: {} was {}'.format(event.src_path, event.event_type), flush=True)
            process = subprocess.Popen(['pkill', 'radiusd'])
            process.wait()
            subprocess.Popen(['/usr/sbin/radiusd', '-X'])


if __name__ == '__main__':
    handler = ConfigChangeHandler()
    observer = Observer()
    observer.schedule(handler, WATCH_DIR, recursive=True)
    observer.start()
    try:
        while True:
            time.sleep(1)
    except:
        observer.stop()
    observer.join()

