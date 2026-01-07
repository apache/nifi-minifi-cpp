function sleep(n)
  local t0 = os.clock()
  while os.clock() - t0 <= n do end
end

function onTrigger(context, session)
  log:info('Sleeping forever')

  while true do
    sleep(1)
  end
end
