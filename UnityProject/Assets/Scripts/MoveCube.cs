using System;
using UnityEngine;

public class MoveCube : MonoBehaviour
{
    private bool _shouldMove;
    private bool _shouldRotate;
    private double _movement;
    private double _rotation;

    private void Start()
    {
        transform.position = MoveCircular(0, 0);
        transform.rotation = Quaternion.Euler(45, 0, 45);
    }

    // Update is called once per frame
    private void Update()
    {
        _shouldMove ^= Input.GetKeyDown(KeyCode.M);
        _shouldRotate ^= Input.GetKeyDown(KeyCode.R);
        transform.position = MoveCircular(Time.deltaTime, Math.PI);
        if (_shouldRotate)
        {
            transform.Rotate(0, 0, 180 * Time.deltaTime);
        }
    }

    private Vector3 MoveCircular(double timing, double scale)
    {
        if (_shouldMove)
        {
            _movement += timing * scale;
        }

        return new Vector3((float)Math.Sin(_movement), (float)Math.Cos(_movement), 0);
    }
}