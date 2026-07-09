function sendCommand(direction)
{
    let formData = new FormData();

    formData.append("direction", direction);

    fetch("/move",
    {
        method:"POST",
        body:formData
    })
    .catch(err =>
    {
        console.error(err);
    });
}