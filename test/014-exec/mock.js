pipy()
    .listen(8080)
        .decodeHttpRequest()
        .replaceMessage(() => new Message('Hello!\n'))
        .encodeHttpResponse()