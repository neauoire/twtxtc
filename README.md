# twtxtc: A twtxt client in C.

## What is `twtxt`?

`twtxt` is a decentralised, minimalist approach to micro-blogging, similar to Twitter but with self-hosted plaintext files. See the [original `twtxt` documentation](http://twtxt.readthedocs.io/en/latest/) for details. `twtxtc` is a `twtxt` client written in the C language.

## Requirements

Build or download an appropriate [`curl` binary](https://curl.haxx.se/download.html) for your system and place it into your `$PATH` or the `twtxtc` folder. If you don't, you won't be able to retrieve your timeline. Sorry.

## Usage

    ./twtxtc [COMMAND]

### Commands

    tweet <text>            Adds <text> to your twtxt timeline.
    timeline                Displays your twtxt timeline.
    following               Gives you a list of all people you follow.
    follow <user> <URL>     Adds the twtxt file from <URL> to your timeline.
                            <user> defines the user name to display.
    unfollow <user>         Removes the user with the display name <user> from your timeline.
    help                    Displays a help screen.

## Building

Use `cmake` to build `twtxtc`:

    cd path/to/source
    cmake .
    cmake --build .

This should be all.

### Compiler flags

For lazyness reasons, `twtxtc` only uses colors in the most prominent place, namely in the timeline: By default you will get an adequate combination of yellow and white in the list. If you don't want to have such nice colors because you prefer boring plain text (and/or you want to process the output automatically), you can use the `NO_COLORS` compiler flag to disable them.

## Configuration

If found, `twtxtc` will use the `.twtxtconfig` file inside your `HOME` directory. (See `twtxtc help` for information on where it should be found.) The `.twtxtconfig` file is meant to be a valid JSON file like this:

    {
        "nickname": "your nickname",
        "twtxtfile": "twtxt.txt",
        "maxlog": 100,
        "spacing": "   ",
        "following": {
            "user_1": "https://example.com/twtxt.txt",
            "user_2": "https://elsewhere.com/tweets.txt"
        }
    }

Possible entries are:

* `nickname`: Your preferred nickname. Only used to filter mentions to other people.
* `twtxtfile`: The location of your `twtxt.txt` file. Defaults to `./twtxt.txt`.
* `maxlog`: The maximum number of entries shown when you view your timeline. Defaults to 100.
* `spacing`: A string value that contains the spacing between the user name and the text when viewing your timeline. Defaults to three spaces.
* `following`: A list of users you follow. Can be managed with the `follow` and `unfollow` commands.

The current limit of the list of users you are following is at 4 KiB. You probably won't reach that limit any time soon.

## TODO

* I'd really like to have Unicode support in `twtxtc`.
* Mentions *could* be made more obvious, e.g. bold formatting or so.

## Licenses

`twtxtc` is licensed under the terms of the [WTFPL](http://wtfpl.net/txt/copying). It uses the lovely [`cJSON`](https://github.com/DaveGamble/cJSON/) library for certain functionalities, basically following the terms of the MIT license. Please read the particular `LICENSE` documents and/or the header files in case you are interested.
